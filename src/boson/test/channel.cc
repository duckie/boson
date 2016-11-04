#include "catch.hpp"
#include "boson/boson.h"
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

static constexpr int nb_iter = 2;
static constexpr int channel_size = 5;

TEST_CASE("Channels", "[channels]") {
  boson::debug::logger_instance(&std::cout);

  SECTION("Simple pipes") {
    boson::run(3, []() {
      using namespace boson;
      channel<int, channel_size> a2b;
      channel<int, channel_size> b2a;
      channel<int, 1> b2c;
      channel<int, 1> c2b;

      // Start a producer
      start(
          [](auto in, auto out) -> void {
            int ack_result = 0;
            for (int i = 0; i < nb_iter; ++i) {
              // Send async
              for (int j = 0; j < channel_size; ++j) {
                out << (i * channel_size + j);
              }
              // get ack
              for (int j = 0; j < channel_size; ++j) {
                in >> ack_result;
                CHECK(ack_result == i * channel_size + j);
              }
            }
          },
          b2a, a2b);

      // Start a router
      start(
          [](auto source_in, auto source_out, auto dest_in, auto dest_out) -> void {
            int result = 0;
            while (result < nb_iter * channel_size - 1) {
              source_in >> result;
              dest_out << result;
              dest_in >> result;
              source_out << result;
            }
          },
          a2b, b2a, c2b, b2c);

      // Start a consumer
      start(
          [](auto in, auto out) -> void {
            int result = 0;
            int expected_value = 0;
            while (result < nb_iter * channel_size - 1) {
              in >> result;
              CHECK(result == expected_value);
              out << result;
              ++expected_value;
            }
          },
          b2c, c2b);
    });
  }
}

