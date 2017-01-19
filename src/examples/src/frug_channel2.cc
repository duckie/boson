#include "stdio.h"
#include "boson/boson.h"
#include "boson/channel.h"
#include "boson/select.h"
#include <string>
#include <fstream>
#include <iostream>

using namespace std::literals;

struct producer {
template <class Duration, class Channel>
void operator() (Duration wait, Channel pipe, int data) const {
  boson::sleep(wait);
  pipe << data;
}
};

int main(int argc, char *argv[]) {
  boson::run(1, []() {
    boson::channel<int, 1> pipe1, pipe2;

    boson::start(
        [](auto in1, auto in2) -> void {
          int result;
          for (int i = 0; i < 2; ++i)
            select_any(
                event_read(in1, result, [&](bool) { std::cout << "Received " << result << "\n"; }),
                event_read(in2, result, [&](bool) { std::cout << "Received " << result << "\n"; }));
        },
        pipe1, pipe2);

    boson::start(producer{}, 500ms, pipe1, 1);
    boson::start(producer{}, 0ms, pipe2, 2);
  });
}
