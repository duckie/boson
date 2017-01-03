#include <iostream>
#include <algorithm>
#include <random>
#include <vector>
#include "boson/event_loop.h"
#include "boson/queues/vectorized_queue.h"
#include "catch.hpp"

TEST_CASE("Vectorized queue - Allocation algorithm", "[queues][vectorized_queue]") {
  constexpr size_t const nb_elements = 1e2;
  std::random_device seed;
  std::mt19937_64 generator{seed()};

  // Instantiate a sparse vector
  boson::queues::vectorized_queue<bool> sparse_instance{nb_elements -
                                                     1};  // -1 to force and end allocation

  // Create a permutation
  std::vector<size_t> indexes;
  indexes.resize(nb_elements);
  for (size_t index = 0; index < nb_elements; ++index) indexes[index] = index;

  std::vector<size_t> expected_allocate_order = indexes;
  std::vector<size_t> permuted_indexes = indexes;
  shuffle(begin(permuted_indexes), end(permuted_indexes), generator);

  // Allocate every cell
  std::vector<size_t> allocate_order;
  for (size_t index = 0; index < nb_elements; ++index)
    allocate_order.emplace_back(sparse_instance.write(0));
  CHECK(allocate_order == expected_allocate_order);

  // Free every cell in random order
  for (auto index : permuted_indexes) sparse_instance.free(index);

  // Reallocate
  allocate_order.clear();
  for (size_t index = 0; index < nb_elements; ++index)
    allocate_order.emplace_back(sparse_instance.write(0));
  reverse(begin(allocate_order), end(allocate_order));
  CHECK(allocate_order == permuted_indexes);
}
