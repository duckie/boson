#include <iostream>
#include <algorithm>
#include <random>
#include <vector>
#include "boson/event_loop.h"
#include "boson/queues/vectorized_queue.h"
#include "catch.hpp"

TEST_CASE("Vectorized queue - Simple behavior", "[queues][vectorized_queue]") {
  // Instantiate a sparse vector
  boson::queues::vectorized_queue<bool> queue;
  queue.write(true);

  // Push then empty
  bool value = false;
  CHECK(queue.read(value));
  CHECK(value);
  value = false;
  CHECK(!queue.read(value));
  CHECK(!queue.read(value));

  // Redo
  value = false;
  queue.write(true);
  CHECK(queue.read(value));
  CHECK(value);
  value = false;
  CHECK(!queue.read(value));
  CHECK(!queue.read(value));
}

TEST_CASE("Vectorized queue - Allocation algorithm", "[queues][vectorized_queue]") {
  constexpr size_t const nb_elements = 1e2;
  std::random_device seed;
  std::mt19937_64 generator{seed()};

  // Instantiate a sparse vector
  boson::queues::vectorized_queue<bool> sparse_instance{
      nb_elements / 2};  // divided by 2 to force dynamic allocations

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

TEST_CASE("Vectorized queue - Queue behavior", "[queues][vectorized_queue]") {
  constexpr size_t const nb_elements = 1e2;
  std::random_device seed;
  std::mt19937_64 generator{seed()};

  // Instantiate a vectorized queue
  boson::queues::vectorized_queue<size_t> sparse_instance{
      nb_elements / 2};  // divided by 2 to force dynamic allocations

  // Create a list a integers
  std::vector<size_t> values;
  for (size_t index = 0; index < nb_elements; ++index) values.push_back(generator());

  // Write them
  for (auto v : values)
    sparse_instance.write(v);

  // Read them
  std::vector<size_t> read_values;
  std::size_t read_value;
  while(sparse_instance.read(read_value))
    read_values.push_back(read_value);

  CHECK(values == read_values);
}

TEST_CASE("Vectorized queue - Queue with suppressed elements", "[queues][vectorized_queue]") {
  constexpr size_t const nb_elements = 1e2+1; // Must be an odd number
  std::random_device seed;
  std::mt19937_64 generator{seed()};

  // Instantiate a vectorized queue
  boson::queues::vectorized_queue<size_t> sparse_instance{
      nb_elements / 2};  // divided by 2 to force dynamic allocations

  // Create a list of integers
  std::vector<size_t> values;
  for (size_t index = 0; index < nb_elements; ++index) values.push_back(index);

  // Write them
  for (auto v : values)
    sparse_instance.write(v);

  // Delete values and store expected ones
  std::vector<size_t> expected_values;
  for(size_t index = 0; index < nb_elements/2; ++index) {
    auto to_read = 2*index + 1;
    expected_values.push_back(values[to_read]);
    sparse_instance.free(to_read+1);
  }
  sparse_instance.free(0);

  // Read them
  std::vector<size_t> read_values;
  std::size_t read_value;
  while(sparse_instance.read(read_value))
    read_values.push_back(read_value);

  CHECK(expected_values == read_values);
}
