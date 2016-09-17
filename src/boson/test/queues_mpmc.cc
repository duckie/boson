#include <iostream>
#include <limits>
#include <random>
#include <thread>
#include "boson/queues/mpmc.h"
#include "catch.hpp"

using namespace boson::queues;

TEST_CASE("Queues - MPMPC - simple push/pop", "[queues][mpmc]") {
  bounded_mpmc<int> test(10);
  test.push(1);
  int result = 0;
  test.pop(result);
  CHECK(result == 1);
}

TEST_CASE("Queues - MPMPC - serial random integers", "[queues][mpmc]") {
  bounded_mpmc<int> test(10);
  test.push(1);
  int result = 0;
  test.pop(result);
  CHECK(result == 1);
}
