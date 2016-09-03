#include "boson/engine.h"
#include <iostream>

int main( int argc, char * argv[])
{
  boson::engine instance(1);
  instance.start([]() {
    for (int i = 0; i < 10; ++i) {
      std::cout << "A: " << i << "\n";
      boson::yield();
    }
  });
  instance.start([]() {
    for (int i = 0; i < 10; ++i) {
      std::cout << "B: " << i << "\n";
      boson::yield();
    }
  });
}
