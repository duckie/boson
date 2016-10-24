#include "catch.hpp"
#include "boson/boson.h"
#include <unistd.h>
#include <iostream>
#include "boson/logger.h"
#include "boson/semaphore.h"

using namespace boson;
using namespace std::literals;

TEST_CASE("Routines - Panic", "[routines][panic]") {
  boson::debug::logger_instance(&std::cout);

  int pipe_fds[2];
  ::pipe(pipe_fds);
  int return_code = 0;

  boson::run(1, [&]() {
    using namespace boson;

    start([&]() {
      char buf[1];
      return_code = boson::read(pipe_fds[0], buf, 1);
    });

    start([&]() {
      boson::fd_panic(pipe_fds[0]);
    });
  });

  CHECK(return_code == boson::code_panic);
}

TEST_CASE("Routines - Semaphores", "[routines][semaphore]") {
  boson::debug::logger_instance(&std::cout);


  boson::run(1, [&]() {
    shared_semaphore sema(1);

    start([](auto sema) -> void {
      bool result = sema.wait();
      CHECK(result == true);
      boson::sleep(10ms);
      sema.post();
      boson::sleep(10ms);
      result = sema.wait();
      CHECK(result == true);
    },sema);

    start([](auto sema) -> void {
      bool result = sema.wait(5ms);
      CHECK(result == false);
      if (!result) {  // To avoid an infinite block if failure (ex valgrind)
        result = sema.wait();
        CHECK(result == true);
        boson::sleep(5ms);
      }
      sema.post();
    },sema);

  });

}

TEST_CASE("Routines - I/O", "[routines][i/o]") {
  boson::debug::logger_instance(&std::cout);

  int pipe_fds[2];
  ::pipe(pipe_fds);

  boson::run(1, [&]() {
    start([](int in) -> void {
      size_t data;
      int result = boson::read(in, &data, sizeof(size_t), 5ms);
      CHECK(result == boson::code_timeout);
      result = boson::read(in, &data, sizeof(size_t));
      CHECK(0 < result);
    },pipe_fds[0]);

    start([](int out) -> void {
      size_t data {0};
      boson::sleep(10ms);
      int result = boson::write(out, &data, sizeof(data));
      CHECK(0 < result);
    },pipe_fds[1]);
  });

  ::close(pipe_fds[0]);
  ::close(pipe_fds[1]);
}
