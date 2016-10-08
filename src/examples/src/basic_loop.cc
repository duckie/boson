#include <iostream>
#include "boson/boson.h"
#include "boson/logger.h"

int main(int argc, char* argv[]) {
  boson::debug::logger_instance(&std::cout);
  boson::engine instance(1, [&]() mutable {
    boson::start([]() {
      for (int i = 0; i < 10; ++i) {
        std::cout << "A: " << i << "\n";
        boson::yield();
      }
    });

    boson::start([]() {
      for (int i = 0; i < 10; ++i) {
        std::cout << "B: " << i << "\n";
        boson::yield();
      }
    });
  });
}
