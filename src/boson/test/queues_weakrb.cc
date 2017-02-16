#include <iostream>
#include <limits>
#include <random>
#include <thread>
#include "boson/queues/weakrb.h"
#include "catch.hpp"

TEST_CASE("Queues - WeakRB - serial random integers", "[queues][weakrb]") {
  constexpr size_t const sample_size = 1e4;

  std::random_device seed;
  std::mt19937_64 generator{seed()};
  std::uniform_int_distribution<int> distribution(std::numeric_limits<int>::min(),
                                                  std::numeric_limits<int>::max());

  std::vector<int> sample;
  sample.reserve(sample_size);
  for (size_t index = 0; index < sample_size; ++index)
    sample.emplace_back(distribution(generator));

  // Create the buffer
  std::vector<int> destination;
  destination.reserve(sample_size);

  boson::queues::weakrb<int> queue(1);

  std::thread t1([&sample, &queue]() {
    for (auto v : sample) {
      bool success = false;
      int value = v;
      // Ugly spin lock, osef
      while (!success) {
        success = queue.write(value);
        std::this_thread::yield();
      }
    }
  });

  std::thread t2([&destination, &queue]() {
    for (size_t index = 0; index < sample_size; ++index) {
      bool success = false;
      int result{};
      // Ugly spin lock, osef
      while (!success) {
        success = queue.read(result);
        std::this_thread::yield();
      }
      destination.emplace_back(result);
    }
  });

  t1.join();
  t2.join();

  CHECK(sample == destination);
}
