#include <iostream>
#include "boson/boson.h"
#include "boson/net/socket.h"
#include "boson/logger.h"

using namespace std::literals;

void listen_client(int fd) {
  std::array<char, 2048> buffer;
  ssize_t nread = 0;
  while ((nread = boson::recv(fd, buffer.data(), buffer.size(), 0))) {
    std::string data(buffer.data(), nread-2);  // cut off "\r\n"
    std::cout << fmt::format("Client {} says: {}\n", fd, data);
    if (data == "quit") break;
  }
  ::shutdown(fd, SHUT_WR);
  boson::close(fd);
}

int main(int argc, char *argv[]) {
  // Set global logger
  boson::debug::logger_instance(&std::cout);

  // Execute a routine communication through channels
  boson::run(1, []() {
    using namespace boson;
    int sockfd = net::create_listening_socket(8080);

    int newsockfd = -1;
    struct sockaddr_in cli_addr;
    socklen_t clilen;
    clilen = sizeof(cli_addr);
    while ((newsockfd = boson::accept(sockfd, (struct sockaddr *)&cli_addr, &clilen))) {
      std::cout << "Opening connection on " << newsockfd << std::endl;
      start(listen_client, newsockfd);
    }
  });
}
