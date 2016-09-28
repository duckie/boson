#include <unistd.h>
#include <chrono>
#include <iostream>
#include "boson/boson.h"
#include "boson/logger.h"
#include "boson/channel.h"

using namespace std::literals;

static constexpr int nb_iter = 2;
static constexpr int channel_size = 5;

int main(int argc, char* argv[]) {
  // Set global logger
  boson::debug::logger_instance(&std::cout);

  // Execute a routine communication through channels
  boson::channel<int,channel_size> a2b;
  boson::channel<int,channel_size> b2a;
  boson::channel<int,0> b2c;
  boson::channel<int,0> c2b;
  boson::engine instance(3);  // 1 is for 1 thread

  // Start a producing routine
  instance.start([a2b,b2a]() mutable {
    int ack_result = 0;
    for (int i = 0; i < nb_iter; ++i) {
      // Send async
      for (int j = 0; j < channel_size; ++j) {
        a2b.push(i*channel_size+j);
        boson::debug::log("A: sent {}.",i*channel_size+j);
      }
      // get ack
      for (int j = 0; j < channel_size; ++j) {
        b2a.pop(ack_result);
        if (ack_result == i*channel_size+j) 
          boson::debug::log("A: ack succeeded.");
      }
    }
  });

  // Start a transmitting routine
  instance.start([a2b,b2a,b2c,c2b]() mutable {
    int result = 0;
    while (result < nb_iter*channel_size - 1) {
      // Transmit
      a2b.pop(result);
      b2c.push(result);
      c2b.pop(result);
      b2a.push(result);
    }
  });

  // Start a consuming routine
  instance.start([b2c,c2b]() mutable {
    int result = 0;
    while (result < nb_iter*channel_size - 1) {
      b2c.pop(result);
      boson::debug::log("C received: {}", result);
      c2b.push(result);
    }
  });
}
