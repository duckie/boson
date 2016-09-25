#include <unistd.h>
#include <chrono>
#include <iostream>
#include "boson/boson.h"
#include "boson/logger.h"

using namespace std::literals;

static constexpr int nb_iter = 10;

int main(int argc, char* argv[]) {
  // Set global logger
  boson::debug::logger_instance(&std::cout);

  // Creates some pipes
  int a2b[2];
  ::pipe(a2b);
  int b2a[2];
  ::pipe(b2a);
  int b2c[2];
  ::pipe(b2c);
  int c2b[2];
  ::pipe(c2b);

  {
    // Execute a routine communication through pipes
    boson::engine instance(1);
    instance.start([&]() {
      int ack_result = 0;
      for (int i = 0; i < nb_iter; ++i) {
        // send data
        boson::write(a2b[1], &i, sizeof(int));
        // Wait for ack
        boson::read(b2a[0], &ack_result, sizeof(int));
        // display info
        if (ack_result == i) boson::debug::log("A: ack succeeded.");
      }

    });
    instance.start([&]() {
      int result = 0;
      while (result < nb_iter - 1) {
        // Get data
        boson::read(a2b[0], &result, sizeof(int));
        // send to C
        boson::write(b2c[1],&result, sizeof(int));
        // Wait ack
        boson::read(c2b[0], &result, sizeof(int));
        // Send ack
        boson::write(b2a[1], &result, sizeof(int));
        
      }
    });
    instance.start([&]() {
      int result = 0;
      while (result < nb_iter - 1) {
        // Get data
        boson::read(b2c[0], &result, sizeof(int));
        // Send ack
        boson::write(c2b[1], &result, sizeof(int));
        // Display info
        boson::debug::log("C received: {}", result);
      }
    });
  }
  ::close(a2b[1]);
  ::close(b2a[0]);
  ::close(a2b[0]);
  ::close(b2a[1]);
  ::close(b2c[1]);
  ::close(c2b[0]);
  ::close(b2c[0]);
  ::close(c2b[1]);
}
