#include <iostream>
#include <limits>
#include <random>
#include <thread>
#include <cassert>
#include "boson/queues/wfqueue.h"
#include "catch.hpp"

namespace {
  static constexpr size_t nb_iter = 1e4;
}

TEST_CASE("Queues - WfQueue - simple case", "[queues][wfqueue]") {
  boson::queues::wfqueue<int*> queue(1);
  int i = 0;
  void* none = queue.pop(0);
  CHECK(none == nullptr);
  queue.push(0,&i);
  void* val = queue.pop(0);
  CHECK(val == &i);
}

TEST_CASE("Queues - WfQueue - sums", "[queues][wfqueue]") {
  //using namespace std;
  //boson::queues::wfqueue<int*> queue(8);
  //array<int,4> vals;
  //array<atomic<size_t>,4> counts;
//
  //void* none = queue.pop(0);
  //CHECK(none == nullptr);
  //queue.push(0,&i);
  //void* val = queue.pop(0);
  //CHECK(val == &i);
}
