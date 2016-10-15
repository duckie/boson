#include <iostream>
#include <set>
#include "boson/boson.h"
#include "boson/channel.h"
#include "boson/net/socket.h"
#include "fmt/format.h"

using namespace boson;

enum class command_type { add, write, remove };

struct command {
  command_type type;
  variant<int, std::pair<int, std::string>> data;
};

void listen_client(int fd, channel<command, 5> chan) {
  std::array<char, 2048> buffer;
  ssize_t nread = 0;
  while ((nread = boson::recv(fd, buffer.data(), buffer.size(), 0))) {
    std::string data(buffer.data(), nread-2);
    if (data.substr(0, 4) == "quit") {
      chan << command{command_type::remove, fd};
      break;
    } else {
      chan << command{command_type::write, {fd, fmt::format("Client {} says: {}\n", fd, data)}};
    }
  }
}

void listen_new_connections(int server_fd, channel<command, 5> chan) {
  struct sockaddr_in cli_addr;
  socklen_t clilen;
  clilen = sizeof(cli_addr);
  for (;;) {
    int newsockfd = boson::accept(server_fd, (struct sockaddr *)&cli_addr, &clilen);
    if (newsockfd < 0 && errno != EAGAIN)
      break;
    chan << command{command_type::add, newsockfd};
  }
}

int main(int argc, char *argv[]) {
  boson::run(1, []() {
    // Create a channel
    channel<command, 5> loop_input;

    // Create socket and list to connections
    using namespace boson;
    int sockfd = net::create_listening_socket(8080);
    start(listen_new_connections, sockfd, loop_input);

    // Main loop
    command new_command;
    std::set<int> conns;
    bool exit = false;
    while (!exit) {
      loop_input >> new_command;
      switch (new_command.type) {
        case command_type::add: {
          int new_conn = new_command.data.get<int>();
          std::cout << "Opening connection on " << new_conn << std::endl;
          conns.insert(new_conn);
          start(listen_client, new_conn, loop_input);
          loop_input << command { command_type::write, {new_conn, fmt::format("Client {} joined.\n",new_conn)}};
        } break;
        case command_type::write: {
          int client = -1;
          std::string data;
          std::tie(client, data) = new_command.data.get<std::pair<int, std::string>>();
          for (auto dest : conns) {
            boson::send(dest, data.c_str(), data.size(), 0);
          }
        } break;
        case command_type::remove: {
          int old_conn = new_command.data.get<int>();
          std::cout << "Closing connection on " << old_conn << std::endl;
          conns.erase(old_conn);
          ::shutdown(old_conn, SHUT_WR);
          ::close(old_conn);
          loop_input << command { command_type::write, { old_conn, fmt::format("Client {} exited.\n",old_conn)} };
          } break;
      }
    };
  });
}
