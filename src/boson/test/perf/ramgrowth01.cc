/**
 * This executable should execute in a stable manner
 * without filling up the RAM
 *
 * This is not actually the case.
 */
#include "boson/boson.h"
#include "boson/channel.h"
#include "boson/select.h"


int main(void) {
  using namespace boson;

  boson::run(1, []() {
    channel<int, 5> chan1;
    channel<int, 5> chan2;

    // Feed without stopping
    start([](auto c) -> void {
      for (;;) c << 0;
    }, chan2);

    // Consume but never chan1
    start([](auto c1, auto c2) -> void {
      int result = 0;
      for (;;) select_any(event_read(c1, result, []() {}), event_read(c2, result, []() {}));
    }, chan1, chan2);
  });

  return 0;
}
