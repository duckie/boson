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
  bounded_mpmc(size_t capacity = 1) : queue_(capacity) {
  }
  bounded_mpmc(bounded_mpmc const&) = delete;
  bounded_mpmc(bounded_mpmc&&) = default;
  bounded_mpmc& operator=(bounded_mpmc const&) = delete;
  bounded_mpmc& operator=(bounded_mpmc&&) = default;
  ~bounded_mpmc() = default;

  template <class... Args>
  bool push(Args&&... args) {
    return queue_.write(std::forward<Args>(args)...);
  };

  bool pop(content_type& element) {
    return queue_.read(element);
  };

  template <class... Args>
  bool blocking_push(Args&&... args) {
    queue_.blockingWrite(std::forward<Args>(args)...);
    return true;
  };

  bool blocking_pop(content_type& element) {
    queue_.blockingRead(element);
    return true;
  };
};

/**
 * unbounded_mpmc is a MPMC unbouded queue
 *
 * Current implementation is just a wrapper around
 * a dynamic folly::MPMCQueue
 *
 * We decide a push that modifies the capacity can be blocking here, we dont care
 * since such calls will not scale
 */
template <class ContentType>
class unbounded_mpmc {
  using content_t = ContentType;
  folly::MPMCQueue<ContentType, std::atomic, true> queue_;

 public:
  using content_type = ContentType;
  unbounded_mpmc(size_t initial_capacity = 1) : queue_(initial_capacity) {
  }
  unbounded_mpmc(unbounded_mpmc const&) = delete;
  unbounded_mpmc(unbounded_mpmc&&) = default;
  unbounded_mpmc& operator=(unbounded_mpmc const&) = delete;
  unbounded_mpmc& operator=(unbounded_mpmc&&) = default;
  ~unbounded_mpmc() = default;

  template <class... Args>
  bool push(Args&&... args) {
    queue_.blockingWrite(std::forward<Args>(args)...);
    return true;
  };

  bool pop(content_type& element) {
    return queue_.read(element);
  };

  bool blocking_pop(content_type& element) {
    queue_.blockingRead(element);
    return true;
  };
};

};  // namespace queues
};  // namespace boson

#endif  // BOSON_QUEUES_MPMC_H_
