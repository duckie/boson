#include <chrono>
#include <iostream>
#include "boson/boson.h"

using namespace std::literals;

int main(int argc, char* argv[]) {
  boson::engine instance(1);
  instance.start([]() {
    for (int i = 0; i < 10; ++i) {
      boson::sleep(1s);
      std::cout << "A: " << i << "\n";
    }
  });
  instance.start([]() {
    for (int i = 0; i < 40; ++i) {
      boson::sleep(250ms);
      std::cout << "B: " << i << "\n";
    }
  });
}
