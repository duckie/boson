#include "catch.hpp"
#include "boson/queues/weakrb.h"
#include <random>
#include <limits>
#include <thread>
#include <iostream>

TEST_CASE("Queues - WeakRB - serial random integers", "[queues][weakrb]") {
  constexpr size_t const sample_size = 1e5;

  std::random_device seed;
  std::mt19937_64 generator{seed()};
  std::uniform_int_distribution<int> distribution(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());

  std::vector<int> sample;
  sample.reserve(sample_size);
  for(size_t index = 0; index < sample_size; ++index)
    sample.emplace_back(distribution(generator));
  
  
  // Create the buffer
  std::vector<int> destination;
  destination.reserve(sample_size);


  boson::queues::weakrb<int,1> queue;
  
  std::thread t1([&sample,&queue]() { 
      for (auto v : sample) {
        bool success = false;
        int value = v;
        while (!success) {
          success = queue.push(value);
        }
      }
  });

  std::thread t2([&destination,&queue]() { 
    for(size_t index = 0; index < sample_size; ++index) {
      bool success = false;
      int result {};
      while (!success) {
        success = queue.pop(result);
      }
      destination.emplace_back(result);
    }
  });

  t1.join();
  t2.join();

  CHECK(sample == destination);
}
