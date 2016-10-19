#ifndef BOSON_QUEUES_MPMC_H_
#define BOSON_QUEUES_MPMC_H_
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <list>
#include <type_traits>
#include <utility>
#include "lcrq.h"
#include "../memory/sparse_vector.h"

namespace boson {
namespace queues {

template <class Value> 
class mpmc {
  lcrq queue_;
  memory::sparse_vector<std::array<char, sizeof(Value)>> storage_;

 public:
  mpmc(int nprocs) : queue_{nprocs} {}
  mpmc(mpmc const &) = delete;
  mpmc(mpmc &&) = default;
  mpmc &operator=(mpmc const &) = delete;
  mpmc &operator=(mpmc &&) = default;
  ~mpmc();

  void write(std::size_t proc_id, Value* v) {
  }

  template <class ... Args>
  void emplace(std::size_t proc_id, Args&& ... args);
  bool read(std::size_t proc_id, Value& v);
};

};  // namespace queues
};  // namespace boson

#endif  // BOSON_QUEUES_MPMC_H_
