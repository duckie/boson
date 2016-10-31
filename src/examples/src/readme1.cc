#include <iostream>
#include <cstdio>
#include "boson/boson.h"

using namespace std::literals;

void timer(int out, bool& stopper) {
  while(!stopper) {
    boson::sleep(1000ms);
    boson::write(out,"Tick !\n",7);
  }
  boson::write(out,"Stop !\n",7);
}

void user_input(int in, bool& stopper) {
  char buffer[1];
  boson::read(in, &buffer, sizeof(buffer));
  stopper = true;
}

int main(int argc, char *argv[]) {
  bool stopper = false;
  boson::run(1, [&stopper]() {
    boson::start(timer, 1, stopper);
    boson::start(user_input, 0, stopper);
  });
}
