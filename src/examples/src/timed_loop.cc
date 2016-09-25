#include <chrono>
#include <iostream>
#include "boson/boson.h"
#include "boson/logger.h"

using namespace std::literals;

int main(int argc, char* argv[]) {
  // Set global logger
  boson::debug::logger_instance(&std::cout);

  boson::engine instance(1);
  instance.start([]() {
    for (int i = 0; i < 10; ++i) {
      boson::sleep(1s);
      boson::debug::log("A: {}",i);
    }
  });
  instance.start([]() {
    for (int i = 0; i < 40; ++i) {
      boson::sleep(250ms);
      boson::debug::log("B: {}",i);
    }
  });
}
