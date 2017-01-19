#include <fstream>
#include <iostream>
#include <string>
#include "boson/boson.h"
#include "boson/channel.h"
#include "boson/select.h"
#include "stdio.h"

using namespace std::literals;
using namespace boson;

int main(int argc, char *argv[]) {
  boson::run(1, []() {
    boson::channel<int, 1> pipe;

    boson::start(
        [](auto in) -> void {
          int result;
          for (int i = 0; i < 2; ++i)
            select_any(
                event_read(0, &result, sizeof(int),
                           [&](ssize_t) { std::cout << "Received on stdin " << result << "\n"; }),
                event_read(in, result,
                           [&](bool) { std::cout << "Received on chan " << result << "\n"; }),
                event_timer(1000ms,  //
                            [&]() { std::cout << "Timeout\n"; }));
        },
        pipe);

    pipe << 2;
  });
}
