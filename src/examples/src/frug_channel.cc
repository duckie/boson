#include "stdio.h"
#include "boson/boson.h"
#include "boson/channel.h"
#include <string>
#include <fstream>
#include <iostream>

int main(int argc, char *argv[]) {
  boson::run(1, []() {
    boson::channel<int, 1> pipe;

    boson::start([](auto input) -> void {
        int i = 0;
        while (input >> i)
          std::cout << "Received " << i << "\n";
    }, pipe);

    boson::start([](auto output) -> void {
        for (int i = 0; i < 5; ++i) {
          output << i;
          std::cout << "Sent " << i << "\n";
        }
        output.close();
    }, pipe);
  });
}
