#include <algorithm>
#include <iostream>
#include <random>
#include <unordered_set>
#include "boson/event_loop.h"
#include "boson/memory/flat_unordered_set.h"
#include "catch.hpp"

TEST_CASE("Flat unordered set", "[memory][flat_unordered_set]") {
  SECTION("Scale test") {
    constexpr size_t const nb_elements = 1e2;
    constexpr size_t const nb_iterations = 1e2;
    std::random_device seed;
    std::mt19937_64 generator{seed()};

    // Instantiate a sparse vector
    boson::memory::flat_unordered_set<int> instance;

    // Create an initial dataset
    std::vector<size_t> indexes;
    indexes.resize(nb_elements);
    for (size_t index = 0; index < nb_elements; ++index) {
      indexes[index] = index;
    }

    // Fill up the instance
    std::uniform_int_distribution<int> dis(0, nb_elements - 1);
    std::unordered_set<int> expected_set;
    for (int index = 0; index < nb_elements * nb_iterations; ++index) {
      int value = dis(generator);
      instance.insert(value);
      expected_set.insert(value);
    }

    // copy the result in an unordered_set
    std::unordered_set<int> result_set;
    for (int value : instance) {
      result_set.insert(value);
    }

    CHECK(result_set == expected_set);
    CHECK(instance.size() == expected_set.size());
  }
}
