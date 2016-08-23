#include "boson/engine.h"
#include <iostream>

int main( int argc, char * argv[])
{
  boson::engine<> instance;
  instance.start([](auto& context) {
    for (int i = 0; i < 10; ++i) {
      std::cout << "A: " << i << "\n";
      boson::yield();
    }
  });
  instance.start([](auto& context) {
    for (int i = 0; i < 10; ++i) {
      std::cout << "B: " << i << "\n";
      boson::yield();
    }
  });
}
