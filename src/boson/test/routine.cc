#include "catch.hpp"
#include "boson/boson.h"
#include <unistd.h>
#include <iostream>
#include "boson/logger.h"
#include "boson/semaphore.h"
#include "boson/select.h"

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

TEST_CASE("Routines - Panic", "[routines][panic]") {
  boson::debug::logger_instance(&std::cout);

  int pipe_fds[2];
  ::pipe(pipe_fds);
  ::fcntl(pipe_fds[0], F_SETFL, ::fcntl(pipe_fds[0], F_GETFD) | O_NONBLOCK);
  ::fcntl(pipe_fds[1], F_SETFL, ::fcntl(pipe_fds[1], F_GETFD) | O_NONBLOCK);
  int return_code = 0;

  boson::run(1, [&]() {
    using namespace boson;

    start([&]() {
      char buf[1];
      return_code = boson::read(pipe_fds[0], buf, 1);
      //CHECK(return_code == -1);
      //CHECK(errno == EINTR);
      
    });

    start([&]() {
      char buf[1] {};
      boson::write(pipe_fds[1], buf, 1);
      //boson::fd_panic(pipe_fds[0]);
    });
  });

}
