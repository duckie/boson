/**
 * This executable should execute in a stable manner
 * without filling up the RAM
 *
 * This is not actually the case.
 */
#include "boson/boson.h"
#include "boson/channel.h"
#include "boson/select.h"

void f1() {
}

void f2() {
}

static constexpr size_t nb_iter = 1e7;

int main(void) {
  using namespace boson;

  boson::run(2, []() {
    channel<int, 5> chan1;
    channel<int, 5> chan2;

    // Feed without stopping
    start([](auto c) -> void {
      for (size_t index = 0; index < nb_iter; ++index)
        c << 1;
      c << 0;
    }, chan2);
    //start([](auto c) -> void {
      //for (size_t index = 0; index < nb_iter; ++index)
        //c << 1;
      //c << 0;
    //}, chan1);

    // Consume but never chan1
    start(
        [](auto c1, auto c2) -> void {
          int result = 0;
          for (;;) {
            select_any(event_read(c1, result, [](bool) { f2(); }),
                       event_read(c2, result, [](bool) { f1(); }));
            if (result == 0) break;
          }
        },
        chan1, chan2);
  });

  return 0;
}
