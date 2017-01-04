#include <iostream>
#include <set>
#include "boson/boson.h"
#include "boson/channel.h"
#include "boson/net/socket.h"
#include "fmt/format.h"
#include "boson/select.h"

using namespace boson;

struct listen_client {
  template <class A, class B, class C>
  void operator()(int fd, A msg_chan, B close_chan, C pilot) {
    std::array<char, 2048> buffer;
    ssize_t nread = 0;
    std::nullptr_t close_flag;

    while (0 < (nread = select_any(                                  //
                    event_recv(fd, buffer.data(), buffer.size(), 0,  //
                               [](int nread) { return nread; }),
                    event_read(pilot, close_flag,  //
                               [](bool success) {
                                 assert(!success);  // Channel should only be closed here
                                 return -1;
                               })))) {
      std::string data(buffer.data(), nread - 2);
      if (data.substr(0, 4) == "quit") {
        break;
      } else {
        msg_chan << fmt::format("Client {} says: {}\n", fd, data);
      }
    }
    close_chan << fd;
  }
};

void broadcast_message(std::set<int> const& connections, std::string const& data) {
  std::shared_ptr<std::string> shared_data { new std::string(data) };
  for (auto dest : connections) {
    boson::start([dest,shared_data]() { boson::send(dest, shared_data->c_str(), shared_data->size(), 0); });
  }
}

int main(int argc, char *argv[]) {
  boson::run(1, []() {
    channel<int, 1> new_connection;
    channel<std::string, 1> messages;
    channel<int, 1> close_connection;
    channel<std::nullptr_t, 1> pilot;  // Used to broadcast end of service

    // Create socket and list to connections
    int sockfd = net::create_listening_socket(8080);
    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);

    // Main loop
    std::array<char, 2048> buffer;
    std::set<int> conns;
    bool exit = false;
    while(!exit) {
      int conn = 0;
      std::string message;
      select_any(                                                     //
          event_accept(sockfd, (struct sockaddr*)&cli_addr, &clilen,  //
                       [&](int conn) {                                //
                         if (0 <= conn) {
                           std::cout << "Opening connection on " << conn << std::endl;
                           conns.insert(conn);
                           start(listen_client{}, conn, messages, close_connection, pilot);
                           broadcast_message(conns, fmt::format("Client {} joined.\n", conn));
                         } else if (errno != EAGAIN) {
                           exit = true;
                         }
                       }),
          event_read(0, buffer.data(), buffer.size(),  // Listen stdin
                     [&](ssize_t nread) {
                       std::string data(buffer.data(), nread - 1);
                       if (data.substr(0, 4) == "quit") {
                         std::string message("Server exited.\n");
                         for (auto dest : conns) {
                           boson::send(dest, message.c_str(), message.size(), 0);
                           ::shutdown(dest, SHUT_WR);
                           ::close(dest);
                         }
                         close_connection.close();  // If client routine trie to use it
                         pilot.close();  // Tells routines to exit
                         exit = true;
                       } else {
                         std::cout << fmt::format("Unknown command \"{}\".\n\n", data);
                       }
                     }),
          event_read(messages, message,
                     [&](bool) {  //
                       broadcast_message(conns, message);
                     }),
          event_read(close_connection, conn,
                     [&](bool) {  //
                       std::cout << "Closing connection on " << conn << std::endl;
                       conns.erase(conn);
                       ::shutdown(conn, SHUT_WR);
                       ::close(conn);
                       broadcast_message(conns, fmt::format("Client {} exited.\n", conn));
                     }));
    };
  });
}
