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

TEST_CASE("Semaphore - Disabling", "[semaphore]") {
  boson::debug::logger_instance(&std::cout);

  SECTION("Normal semaphore") {
    boson::run(1, [&]() {
      shared_semaphore sema(1);

      start([](auto sema) -> void {
        bool result = sema.wait();
        CHECK((result == true));
        boson::sleep(time_factor()*10ms);
        sema.post();
        boson::sleep(time_factor()*10ms);
        result = sema.wait();
        CHECK(result == true);
      },sema);

      start([](auto sema) -> void {
        bool result = sema.wait(time_factor()*5ms);
        REQUIRE(result == false);
        result = sema.wait();
        CHECK(result == true);
        boson::sleep(time_factor()*5ms);
        sema.post();
      },sema);
    });
  }

  SECTION("Timedout semaphore") {
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
        REQUIRE(result == false);
        result = sema.wait();
        CHECK(result == true);
        boson::sleep(time_factor()*5ms);
        sema.post();
      },sema);
    });
  }

  SECTION("Disabled semaphore") {
    boson::run(1, [&]() {
      shared_semaphore sema(0);

      start([](auto sema) -> void {
        semaphore_return_value result = sema.wait();
        CHECK(result == semaphore_return_value::disabled);
      },sema);

      start([](auto sema) -> void {
        sema.disable();
      },sema);
    });
  }
}
