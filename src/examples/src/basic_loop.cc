#include <iostream>
#include "boson/boson.h"
#include "boson/logger.h"

int main(int argc, char* argv[]) {
  boson::debug::logger_instance(&std::cout);

  boson::run(1, []() {
    using namespace boson;

    start([]() {
      for (int i = 0; i < 10; ++i) {
        std::cout << "A: " << i << "\n";
        boson::yield();
      }
    });

    start([]() {
      for (int i = 0; i < 10; ++i) {
        std::cout << "B: " << i << "\n";
        boson::yield();
      }
    });
  });
}
