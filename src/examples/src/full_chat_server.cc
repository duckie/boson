#include <netinet/in.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <unistd.h>
#include <chrono>
#include <iostream>
#include <set>
#include "boson/boson.h"
#include "boson/channel.h"
#include "boson/exception.h"
#include "boson/net/socket.h"
#include "boson/logger.h"

using namespace std::literals;
using namespace boson;

enum class command_type { add, write, remove, quit };

struct command {
  command_type type;
  json_backbone::variant<int, std::pair<int, std::string>> data;
};

void listen_client(int fd, channel<command, 5> main_loop) {
  std::array<char, 2048> buffer;
  ssize_t nread = 0;
  while (0 < (nread = boson::recv(fd, buffer.data(), buffer.size(), 0))) {
    std::string data(buffer.data(), nread-1);
    if (data.substr(0, 4) == "quit") {
      break;
    } else {
      main_loop.write(command{command_type::write, {fd, fmt::format("Client {} says: {}\n", fd, data)}});
    }
  }
  main_loop.write(command{command_type::remove, fd});
}

void listen_server_command(int std_in, int std_out, channel<command, 5> main_loop) {
  std::array<char, 2048> buffer;
  ssize_t nread = 0;
  boson::write(std_out,"\n> ",3);
  while (0 < (nread = boson::read(std_in, buffer.data(), buffer.size()))) {
    std::string data(buffer.data(), nread-1);
    if (data.substr(0, 4) == "quit") {
      main_loop.write(command{command_type::quit, 0});
      break;
    } else {
      std::string message(fmt::format("Unknown command \"{}\".\n",data));
      boson::write(std_out,message.c_str(), message.size());
      boson::write(std_out,"\n> ",3);
    }
  }
}

void listen_new_connections(int server_fd, channel<command, 5> chan) {
  struct sockaddr_in cli_addr;
  socklen_t clilen;
  clilen = sizeof(cli_addr);
  int newsockfd = -1;
  while ((newsockfd = boson::accept(server_fd, (struct sockaddr *)&cli_addr, &clilen))) {
    if (newsockfd < 0 && errno != EAGAIN)
      break;
    chan.write(command{command_type::add, newsockfd});
  }
  ::shutdown(server_fd, SHUT_WR);
  ::close(server_fd);
}

int main(int argc, char *argv[]) {
  // Set global logger
  boson::debug::logger_instance(&std::cout);

  // Execute a routine communication through channels
  boson::run(8, []() {
    // Create a channel
    channel<command, 5> loop_input;

    // Create socket and list to connections
    using namespace boson;
    int sockfd = boson::net::create_listening_socket(8080);
    start(listen_new_connections, sockfd, loop_input);

    // Listen stdout for commands
    start(listen_server_command, 0, 1, loop_input);

    // Main loop
    command new_command;
    std::set<int> conns;
    bool exit = false;
    while (!exit) {
      loop_input.read(new_command);
      switch (new_command.type) {
        case command_type::add: {
          int new_conn = new_command.data.get<int>();
          //std::cout << "Opening connection on " << new_conn << std::endl;
          conns.insert(new_conn);
          start(listen_client, new_conn, loop_input);
          loop_input.write(command { command_type::write, {new_conn, fmt::format("Client {} joined.\n",new_conn)}});
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
          //std::cout << "Closing connection on " << old_conn << std::endl;
          conns.erase(old_conn);
          ::shutdown(old_conn, SHUT_WR);
          ::close(old_conn);
          loop_input.write(command { command_type::write, { old_conn, fmt::format("Client {} exited.\n",old_conn)} });
          } break;
        case command_type::quit: {
          std::cout << "Closing server." << std::endl;
          std::string message("Server exited.\n");
          for (auto dest : conns) {
            boson::send(dest, message.c_str(), message.size(), 0);
            ::shutdown(dest, SHUT_WR);
            ::close(dest);
            boson::fd_panic(dest);
          }
          boson::fd_panic(sockfd);
          exit = true;
         } break;
      }
    };
  });
}
