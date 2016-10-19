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


  //boson::run(1, [&]() {
    //shared_semaphore sema(1);
//
    //start([](auto sema) {
      //bool result = sema.wait();
      //debug::log("Yeah 1 {}",result);
    //},dup(sema));
//
    //start([](auto sema) {
      ////sema.wait();
      //bool result = sema.wait(2000ms);
      //debug::log("Yeah 2 {}", result);
    //},dup(sema));
  //});

}
