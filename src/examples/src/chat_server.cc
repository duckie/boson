#include <netinet/in.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <unistd.h>
#include <chrono>
#include <iostream>
#include "boson/boson.h"
#include "boson/channel.h"
#include "boson/exception.h"
#include "boson/logger.h"
#include <set>

using namespace std::literals;
using namespace boson;

enum class command_type {
  add,
  write,
  remove
};

struct command {
  command_type type;
  json_backbone::variant<int, std::pair<int,std::string>> data;
};

int create_listening_socket() {
  int sockfd, portno;
  char buffer[256];
  struct sockaddr_in serv_addr;
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  fcntl(sockfd, F_SETFL, O_NONBLOCK);
  if (sockfd < 0) throw boson::exception("ERROR opening socket");
  bzero((char *)&serv_addr, sizeof(serv_addr));
  portno = 8080;
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);

  // re use socket
  int yes = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
    throw boson::exception("setsockopt");

  if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    throw boson::exception("ERROR on binding");
  listen(sockfd, 5);
  return sockfd;
}

void listen_client(int fd, channel<command,5> main_loop) {
  std::array<char, 2048> buffer;
  ssize_t nread = 0;
  while ((nread = boson::recv(fd, buffer.data(), buffer.size(), 0))) {
    std::string data(buffer.data(), nread);
    std::cout << fmt::format("Client {} says: {}", fd, data);
    if (data.substr(0, 4) == "quit") {
      main_loop.write(command { command_type::remove, fd });
      break;
    }
    else {
      main_loop.write(command { command_type::write, {fd,data.substr(0,data.size()-1)} });
    }
  }
}

void server_loop(channel<command,5> channel) {
  command new_command;
  std::set<int> conns;
  while(channel.read(new_command)) {
    switch(new_command.type) {
      case command_type::add: {
        int new_conn = new_command.data.get<int>();
        std::cout << "Opening connection on " << new_command.data.get<int>() << std::endl;
        conns.insert(new_conn);
        start(listen_client, new_conn, dup(channel));
        //channel.write(command { command_type::write, {new_conn, "just joined."}});
      } break;
      case command_type::write: {
        int client = -1;
        std::string data;
        std::tie(client,data) = new_command.data.get<std::pair<int,std::string>>();
        auto message = fmt::format("Client {} says: \"{}\"", client, data);
        for(auto dest : conns) {
          boson::send(dest,message.c_str(), message.size(),0);
        }
      } break;
      case command_type::remove:
        int old_conn = new_command.data.get<int>();
        conns.erase(old_conn);
        ::shutdown(old_conn, SHUT_WR);
        ::close(old_conn);
        //channel.write(command { command_type::write, { old_conn, "just exited"} });
        break;
    }
  };
}


int main(int argc, char *argv[]) {
  // Set global logger
  boson::debug::logger_instance(&std::cout);

  // Execute a routine communication through channels
  boson::run(1, []() {
    // Create a channel
    channel<command,5> loop_input;

    // Start the loop
    start(server_loop, dup(loop_input));

    // Create socket and list to connections
    using namespace boson;
    int sockfd = create_listening_socket();
    struct sockaddr_in cli_addr;
    socklen_t clilen;
    clilen = sizeof(cli_addr);
    int newsockfd = -1;
    while ((newsockfd = boson::accept(sockfd, (struct sockaddr *)&cli_addr, &clilen))) {
      loop_input.write(command {command_type::add, newsockfd });
    }
  });
}
