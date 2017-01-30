#include "catch.hpp"
#include "boson/boson.h"
#include "boson/syscalls.h"
#include "boson/net/socket.h"
#include <unistd.h>
#include <iostream>
#include "boson/logger.h"
#include "boson/semaphore.h"
#include "boson/select.h"
#ifdef BOSON_USE_VALGRIND
#include "valgrind/valgrind.h"
#endif 

using namespace boson;
using namespace std::literals;

namespace {
inline int time_factor() {
#ifdef BOSON_USE_VALGRIND
  return RUNNING_ON_VALGRIND ? 10 : 1;
#else
  return 1;
#endif 
}
}

TEST_CASE("Sockets - Simple accept/connect", "[syscalls][sockets][accept][connect]") {
  boson::debug::logger_instance(&std::cout);

  SECTION("Simple pipes") {

    boson::run(1, [&]() {
      start(
          []() -> void {
            // Listen on a port
            int listening_socket = boson::net::create_listening_socket(10101);
            struct sockaddr_in cli_addr;
            socklen_t clilen = sizeof(cli_addr);
            int new_connection = boson::accept(listening_socket, (struct sockaddr*)&cli_addr, &clilen);
          });

      start(
          []() -> void {
            struct sockaddr_in cli_addr;
            cli_addr.sin_addr.s_addr = ::inet_addr("127.0.0.1");
            cli_addr.sin_family = AF_INET;
            cli_addr.sin_port = htons(10101);
            socklen_t clilen = sizeof(cli_addr);
            int sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
            ::fcntl(sockfd, F_SETFL, O_NONBLOCK);
            int new_connection = boson::connect(sockfd, (struct sockaddr*)&cli_addr, clilen);
            CHECK(0 == new_connection);
          });
    });
  }
}
