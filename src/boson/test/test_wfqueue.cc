#include <iostream>
#include <limits>
#include <random>
#include <thread>
#include "boson/queues/wfqueue.h"
#include "catch.hpp"

TEST_CASE("Queues - WfQueue - serial random integers", "[queues][wfqueue]") {
  boson::queues::wfqueue<int> queue(10);
  queue.push(0,1);
}
