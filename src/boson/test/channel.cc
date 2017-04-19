#include "catch.hpp"
#include "boson/boson.h"
#include <unistd.h>
#include "boson/logger.h"
#include "boson/semaphore.h"
#include "boson/select.h"
#include <iostream>
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

static constexpr int nb_iter = 10;
static constexpr int channel_size = 5;

TEST_CASE("Channels", "[channels]") {
  boson::debug::logger_instance(&std::cout);

  std::vector<int> ids;
  std::vector<int> acks;

  SECTION("Simple pipes - Producer/Consumer") {
    boson::run(3, [&]() {
      using namespace boson;
      channel<int, channel_size> a2b;
      channel<int, channel_size> b2a;
      //channel<int, 1> b2c;
      //channel<int, 1> c2b;

      // Start a producer
      start(
          [&](auto in, auto out) -> void {
            int ack_result = 0;
            for (int i = 0; i < nb_iter; ++i) {
              // Send async
              for (int j = 0; j < channel_size; ++j) {
                out << (i * channel_size + j);
              }
              // get ack
              for (int j = 0; j < channel_size; ++j) {
                in >> ack_result;
                acks.push_back(ack_result);
              }
            }
          },
          b2a, a2b);

      // Start a consumer
      start(
          [&](auto in, auto out) -> void {
            int result = 0;
            while (result < nb_iter * channel_size - 1) {
              in >> result;
              ids.push_back(result);
              out << result;
            }
          },
          a2b, b2a);
    });
    
  }

  SECTION("Simple pipes - Producer/Router/Consumer") {
    boson::run(3, [&]() {
      using namespace boson;
      channel<int, channel_size> a2b;
      channel<int, channel_size> b2a;
      channel<int, 1> b2c;
      channel<int, 1> c2b;

      // Start a producer
      start(
          [&](auto in, auto out) -> void {
            int ack_result = 0;
            for (int i = 0; i < nb_iter; ++i) {
              // Send async
              for (int j = 0; j < channel_size; ++j) {
                out << (i * channel_size + j);
              }
              // get ack
              for (int j = 0; j < channel_size; ++j) {
                in >> ack_result;
                acks.push_back(ack_result);
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
          [&](auto in, auto out) -> void {
            int result = 0;
            while (result < nb_iter * channel_size - 1) {
              in >> result;
              ids.push_back(result);
              out << result;
            }
          },
          b2c, c2b);
    });
  }

  SECTION("Simple closed channels") {
    boson::run(3, [&]() {
      using namespace boson;
      channel<int, channel_size> a2b;
      channel<int, channel_size> b2c;

      // Start a producer
      start(
          [&](auto out) -> void {
            int ack_result = 0;
            for (int i = 0; i < nb_iter * channel_size; ++i) {
              out << i;    
            }
            out.close();
          },
          a2b);

      // Start a router
      start(
          [&](auto in, auto out) -> void {
            int result = 0;
            while (in >> result) {
              ids.push_back(result);
              out << result;
            }
            out.close();
          },
          a2b, b2c);

      start(
          [&](auto in) -> void {
            int result = 0;
            while (in >> result) {
              acks.push_back(result);
            }
          },
          b2c);
    });
  }

  // Playd one time for each section
  std::vector<int> expected(nb_iter * channel_size,0);
  for(int i = 0; i < nb_iter * channel_size;++i) {
    expected[i] = i;
  }
  CHECK(ids == expected);
  CHECK(acks == expected);
}

TEST_CASE("Empty channels", "[channels]") {
  boson::debug::logger_instance(&std::cout);
  SECTION("Close an empty channel") {
    boson::run(3, [&]() {
      using namespace boson;
      channel<int, channel_size> chan;

      chan.close();
      int result = 0;
      CHECK(channel_result_value::closed == (chan << result));
      CHECK(channel_result_value::closed == (chan >> result));
    });
  }

  SECTION("Close an empty channel of size 0") {
    boson::run(3, [&]() {
      using namespace boson;
      channel<std::nullptr_t, channel_size> chan;

      chan.close();
      std::nullptr_t result = 0;
      CHECK(channel_result_value::closed == (chan << nullptr));
      CHECK(channel_result_value::closed == (chan >> result));
    });
  }
}

TEST_CASE("Timeouts on channels", "[channels]") {
  boson::debug::logger_instance(&std::cout);
  SECTION("Timeout an empty channel") {
    boson::run(3, [&]() {
      using namespace boson;
      channel<int, channel_size> chan;

      for(int index = 0; index < channel_size; ++index) {
        chan.write(index);
      }
      CHECK(chan.write(0,time_factor()) == channel_result_value::timedout);
      
      int dummy = 0;
      for(int index = 0; index < channel_size; ++index) {
        chan.read(dummy);
        CHECK(index == dummy);
      }
      CHECK(chan.read(dummy,time_factor()) == channel_result_value::timedout);
    });
  }

  SECTION("Timeout an empty channel of nullptr") {
    boson::run(3, [&]() {
      using namespace boson;
      channel<std::nullptr_t, channel_size> chan;

      for(int index = 0; index < channel_size; ++index) {
        chan.write(nullptr);
      }
      CHECK(chan.write(0,time_factor()) == channel_result_value::timedout);
      
      std::nullptr_t dummy {};
      for(int index = 0; index < channel_size; ++index) {
        chan.read(dummy);
      }
      CHECK(chan.read(dummy,time_factor()) == channel_result_value::timedout);
    });
  }
}
