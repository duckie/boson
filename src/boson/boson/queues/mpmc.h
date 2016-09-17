#ifndef BOSON_QUEUES_MPMC_H_
#define BOSON_QUEUES_MPMC_H_

#include <atomic>
#include <cstdint>
#include <type_traits>
#include "folly/MPMCQueue.h"

namespace boson {
namespace queues {

/**
 * mpmpc is a MPMC queue
 *
 * Current implementation is just a wrapper around
 * folly::MPMCQueue non dynamic
 */
template <class ContentType>
class bounded_mpmc {
  using content_t = ContentType;
  folly::MPMCQueue<ContentType, std::atomic, false> queue_;

 public:
  using content_type = ContentType;
  bounded_mpmc(size_t capacity = 1)
    : queue_(capacity)
  {}
  bounded_mpmc(bounded_mpmc const&) = delete;
  bounded_mpmc(bounded_mpmc&&) = default;
  bounded_mpmc& operator=(bounded_mpmc const&) = delete;
  bounded_mpmc& operator=(bounded_mpmc&&) = default;
  ~bounded_mpmc() = default;

  template <class ... Args>
  bool push(Args&& ... args) {
    return queue_.write(std::forward<Args>(args)...);
  };

  bool pop(content_type& element) {
    return queue_.read(element);
  };

  template <class ... Args>
  bool blocking_push(Args&& ... args) {
    return queue_.blockingWrite(std::forward<Args>(args)...);
  };

  bool blocking_pop(content_type& element) {
    return queue_.blockingRead(element);
  };
};

};  // namespace queues
};  // namespace boson

#endif  // BOSON_QUEUES_MPMC_H_
