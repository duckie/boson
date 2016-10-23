#include <unistd.h>
#include <chrono>
#include <iostream>
#include "boson/boson.h"
#include "boson/channel.h"
#include "boson/logger.h"

using namespace std::literals;

static constexpr int nb_iter = 2;
static constexpr int channel_size = 5;

int main(int argc, char* argv[]) {
  // Set global logger
  boson::debug::logger_instance(&std::cout);

  // Execute a routine communication through channels

  // Launch an engine with 3 threads
  boson::run(3, []() {
    using namespace boson;
    channel<int, channel_size> a2b;
    channel<int, channel_size> b2a;
    channel<int, 0> b2c;
    channel<int, 0> c2b;

    // Start a producer
    start([](auto in, auto out) -> void {
      int ack_result = 0;
      for (int i = 0; i < nb_iter; ++i) {
        // Send async
        for (int j = 0; j < channel_size; ++j) {
          out << (i * channel_size + j);
          boson::debug::log("A: sent {}.", i * channel_size + j);
        }
        // get ack
        for (int j = 0; j < channel_size; ++j) {
          in >> ack_result;
          if (ack_result == i * channel_size + j) boson::debug::log("A: ack succeeded.");
        }
      }
    }, b2a, a2b);

    // Start a router
    start([](auto source_in, auto source_out, auto dest_in, auto dest_out) -> void{
      int result = 0;
      while (result < nb_iter * channel_size - 1) {
        source_in >> result;
        dest_out << result;
        dest_in >> result;
        source_out << result;
      }
    },a2b, b2a, c2b, b2c);

    // Start a consumer
    start([](auto in, auto out) -> void {
      int result = 0;
      while (result < nb_iter * channel_size - 1) {
        in >> result;
        boson::debug::log("C received: {}", result);
        out << result;
      }
    }, b2c, c2b);
  });
}
