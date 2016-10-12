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
    start(
        [](auto in, auto out) {
          int ack_result = 0;
          for (int i = 0; i < nb_iter; ++i) {
            // Send async
            for (int j = 0; j < channel_size; ++j) {
              out.write(i * channel_size + j);
              boson::debug::log("A: sent {}.", i * channel_size + j);
            }
            // get ack
            for (int j = 0; j < channel_size; ++j) {
              in.read(ack_result);
              if (ack_result == i * channel_size + j) boson::debug::log("A: ack succeeded.");
            }
          }
        },
        dup(b2a), dup(a2b));

    // Start a router
    start(
        [](auto source_in, auto source_out, auto dest_in, auto dest_out) {
          int result = 0;
          while (result < nb_iter * channel_size - 1) {
            source_in.read(result);
            dest_out.write(result);
            dest_in.read(result);
            source_out.write(result);
          }
        },
        dup(a2b), dup(b2a), dup(c2b), dup(b2c));

    // Start a consumer
    start(
        [](auto in, auto out) {
          int result = 0;
          while (result < nb_iter * channel_size - 1) {
            in.read(result);
            boson::debug::log("C received: {}", result);
            out.write(result);
          }
        },
        dup(b2c), dup(c2b));
  });
}
