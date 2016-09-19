#include <iostream>
#include <limits>
#include <random>
#include <thread>
#include "boson/queues/mpmc.h"
#include "catch.hpp"

using namespace boson::queues;

TEST_CASE("Queues - Bounded MPMPC - simple push/pop", "[queues][bounded_mpmc]") {
  bounded_mpmc<int> test(1);
  bool attempt1 = test.push(1);
  bool attempt2 = test.push(1);
  CHECK(attempt1);
  CHECK(!attempt2);
  int result = 0;
  test.pop(result);
  CHECK(result == 1);
}

TEST_CASE("Queues - Bounded MPMPC - serial random integers", "[queues][bounded_mpmc]") {
  bounded_mpmc<int> test(10);
  test.push(1);
  int result = 0;
  test.pop(result);
  CHECK(result == 1);
}

TEST_CASE("Queues - Unbounded MPMPC - serial random integers", "[queues][unbounded_mpmc]") {
  unbounded_mpmc<int> test(10);
  test.push(1);
  int result = 0;
  test.pop(result);
  CHECK(result == 1);
}
