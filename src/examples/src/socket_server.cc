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

using namespace std::literals;

void listen_client(int fd) {
  std::array<char, 2048> buffer;
  ssize_t nread = 0;
  while ((nread = boson::recv(fd, buffer.data(), buffer.size(), 0))) {
    std::string data(buffer.data(), nread);
    std::cout << fmt::format("Client {} says: {}", fd, data);
    if (data.substr(0, 4) == "quit") break;
  }
  ::shutdown(fd, SHUT_WR);
  ::close(fd);
}

int main(int argc, char *argv[]) {
  // Set global logger
  boson::debug::logger_instance(&std::cout);

  // Execute a routine communication through channels
  boson::run(1, []() {
    using namespace boson;

    int sockfd, newsockfd, portno;
    socklen_t clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
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
    clilen = sizeof(cli_addr);
    while ((newsockfd = boson::accept(sockfd, (struct sockaddr *)&cli_addr, &clilen))) {
      std::cout << "Opening connection on " << newsockfd << std::endl;
      start(listen_client, newsockfd);
    }
  });
}
