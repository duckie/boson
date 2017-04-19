#include "catch.hpp"
#include "boson/boson.h"
#include "boson/syscalls.h"
#include <unistd.h>
#include <iostream>
#include <cstdio>
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

TEST_CASE("Syscalls - File opening", "[syscalls]") {
  boson::debug::logger_instance(&std::cout);

  std::array<char, L_tmpnam> filename_buffer;
  auto fileName1 = std::tmpnam(filename_buffer.data());
  REQUIRE(fileName1 != nullptr);

  boson::run(1, [&]() {
    auto fd1 = boson::creat(fileName1, 00400);
    REQUIRE(0 <= fd1);
    int rc = boson::close(fd1);
    CHECK(rc == 0);
  });

  boson::run(1, [&]() {
    auto fd1 = boson::open(fileName1, O_RDONLY, 00400);
    REQUIRE(0 <= fd1);
    int rc = boson::close(fd1);
    CHECK(rc == 0);
  });

  boson::run(1, [&]() {
    auto fd1 = boson::open(fileName1, O_RDONLY);
    REQUIRE(0 <= fd1);
    int rc = boson::close(fd1);
    CHECK(rc == 0);
  });

  boson::run(1, [&]() {
    int pipeFds[2] {};
    int rc = boson::pipe2(pipeFds, 0);
    CHECK(rc == 0);
    rc = boson::close(pipeFds[0]);
    CHECK(rc == 0);
    rc = boson::close(pipeFds[1]);
    CHECK(rc == 0);
  });
}
