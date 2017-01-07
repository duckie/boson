#include "catch.hpp"
#include "boson/boson.h"
#include <unistd.h>
#include "boson/logger.h"
#include "boson/shared_buffer.h"
#include <iostream>

using namespace boson;

/**
 * This tests abuses of its knowledge of the framework
 * to check that the buffer is effectively shared.
 *
 * The fact that data is shared between routines through it is 
 * NOT a feature and should NOT be used.
 */
TEST_CASE("Shared buffer", "[shared_buffer]") {
  boson::debug::logger_instance(&std::cout);
  boson::run(1, []() {
    boson::shared_buffer<std::array<int,4>> buffer; 
    std::array<int,4> base {1,2,3,0};
    buffer.get() = base;

    for (int i = 1; i < 10; ++i)
      boson::start([](int index) {
        boson::shared_buffer<std::array<int, 4>> buffer;
        auto& data = buffer.get();
        CHECK(data[0] == 1);
        CHECK(data[1] == 2);
        CHECK(data[2] == 3);
        CHECK(data[3] == index-1);
        data[3] = index;
        boson::yield();
      },i);

    auto& data = buffer.get();
    CHECK(data == (std::array<int,4>({1,2,3,0})));
  });
}

