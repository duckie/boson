#include <iostream>
#include <limits>
#include <random>
#include <thread>
#include "boson/queues/wfqueue.h"
#include "catch.hpp"

TEST_CASE("Queues - WeakRB - serial random integers", "[queues][wfqueue]") {
  boson::queues::wfqueue<int> queue;
  queue.enqueue(1);
}
