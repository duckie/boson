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
  return 1
#endif 
}
}

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

  SECTION("Normal semaphore") {
    boson::run(1, [&]() {
      shared_semaphore sema(1);

      start([](auto sema) -> void {
        bool result = sema.wait();
        CHECK(result == true);
        boson::sleep(time_factor()*10ms);
        sema.post();
        boson::sleep(time_factor()*10ms);
        result = sema.wait();
        CHECK(result == true);
      },sema);

      start([](auto sema) -> void {
        bool result = sema.wait(time_factor()*5ms);
        CHECK(result == false);
        if (!result) {  // To avoid an infinite block if failure (ex valgrind)
          result = sema.wait();
          CHECK(result == true);
          boson::sleep(time_factor()*5ms);
        }
        sema.post();
      },sema);
    });
  }


  SECTION("Disabled semaphore") {
    boson::run(1, [&]() {
      shared_semaphore sema(1);

      start([](auto sema) -> void {
        bool result = sema.wait();
        CHECK(result == true);
        boson::sleep(time_factor()*10ms);
        sema.post();
        boson::sleep(time_factor()*10ms);
        result = sema.wait();
        CHECK(result == true);
      },sema);

      start([](auto sema) -> void {
        bool result = sema.wait(time_factor()*5ms);
        CHECK(result == false);
        if (!result) {  // To avoid an infinite block if failure (ex valgrind)
          result = sema.wait();
          CHECK(result == true);
          boson::sleep(time_factor()*5ms);
        }
        sema.post();
      },sema);
    });
  }
}
