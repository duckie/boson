#include <iostream>
#include <set>
#include "boson/boson.h"
#include "boson/channel.h"
#include "boson/shared_buffer.h"
#include "boson/net/socket.h"
#include "fmt/format.h"
#include "boson/select.h"
#include <fcntl.h>
#include <signal.h>

using namespace boson;

struct listen_client {
  template <class A, class B>
  void operator()(int fd, A msg_chan, B close_chan) {
    boson::shared_buffer<std::array<char, 2048>> buffer;
    ssize_t nread = 0;
    while (0 < (nread = boson::read(fd, buffer.get().data(), buffer.get().size()))) {
      std::string data(buffer.get().data(), nread - 2);
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
    // Ignore SIGPIPE
    struct sigaction action {SIG_IGN,0,0};
    ::sigaction(SIGPIPE, &action, nullptr);

    channel<int, 1> new_connection;
    channel<std::string, 1> messages;
    channel<int, 1> close_connection;

    // Create socket and list to connections
    int sockfd = net::create_listening_socket(8080);
    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);

    // Make stdin non blocking
    ::fcntl(0, F_SETFL, ::fcntl(0, F_GETFD) | O_NONBLOCK);

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
                           ::fcntl(conn, F_SETFL, ::fcntl(conn, F_GETFD) | O_NONBLOCK);
                           conns.insert(conn);
                           start(listen_client{}, conn, messages, close_connection);
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
                           boson::close(dest);  // Will interrupt blocked routines
                         }
                         close_connection.close();  // If client routine tries to use it
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
                       boson::close(conn);
                       broadcast_message(conns, fmt::format("Client {} exited.\n", conn));
                     }));
    };
  });
}
