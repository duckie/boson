#ifndef BOSON_SEMAPHORE_H_
#define BOSON_SEMAPHORE_H_

#include <memory>
#include <chrono>
#include "internal/routine.h"
#include "internal/thread.h"
#include "queues/lcrq.h"

namespace boson {

template <class ContentType, std::size_t Size, class Func>
class event_channel_read_storage;
template <class ContentType, std::size_t Size, class Func>
class event_channel_write_storage;

enum class semaphore_return_value { ok, timedout, disabled };

struct semaphore_result {
  semaphore_return_value value;

  inline operator bool() const {
    return value == semaphore_return_value::ok;
  }

  inline operator semaphore_return_value() const {
    return value;
  }
};

/**
 * Semaphore for routines only
 *
 * The boson semaphore may only be used from routines.
 */
class semaphore : public std::enable_shared_from_this<semaphore> {
  friend class internal::thread;
  friend class internal::routine;
  template <class Content, std::size_t Size, class Func>
  friend class event_channel_read_storage;
  template <class Content, std::size_t Size, class Func>
  friend class event_channel_write_storage;

  static constexpr int disabling_threshold = 0x40000000;
  static constexpr int disabled_standpoint = 0x60000000;

  using queue_t = queues::lcrq;
  queue_t waiters_;
  std::atomic<int> counter_;

  /**
   * tries to unlock a waiter
   *
   * this is defered to the thread maintaining said routine. so we might
   * be suspended then unlocked right after.
   *
   * returns true if the poped thread is not the current or if
   * none could be poped
   */
  bool pop_a_waiter(internal::thread* current = nullptr);

 public:
  semaphore(int capacity);
  semaphore(semaphore const&) = delete;
  semaphore(semaphore&&) = default;
  semaphore& operator=(semaphore const&) = delete;
  semaphore& operator=(semaphore&&) = default;
  virtual ~semaphore();

  /**
   * Disable the semaphore for future uses of wait
   *
   * Posts will be accepted, no wait will be allowed anymore
   */
  inline void disable();

  /**
   * takes a semaphore ticker if it could, otherwise suspend the routine until a ticker is available
   */
  semaphore_result wait(int timeout_ms = -1);

  inline semaphore_result wait(std::chrono::milliseconds);
  /**
   * give back semaphore ticket. Always non blocking
   */
  void post();
};

void semaphore::disable() {
  counter_.store(disabled_standpoint, std::memory_order_release);
}

semaphore_result semaphore::wait(std::chrono::milliseconds timeout) {
  return wait(timeout.count());
}

/**
 * shared_semaphore is a wrapper for shared_ptr of a semaphore
 */
class shared_semaphore {
  friend class internal::thread;
  friend class internal::routine;
  template <class Content, std::size_t Size, class Func>
  friend class event_channel_read_storage;
  template <class Content, std::size_t Size, class Func>
  friend class event_channel_write_storage;
  std::shared_ptr<semaphore> impl_;

 public:
  inline shared_semaphore(int capacity);
  shared_semaphore(shared_semaphore const&) = default;
  shared_semaphore(shared_semaphore&&) = default;
  shared_semaphore& operator=(shared_semaphore const&) = default;
  shared_semaphore& operator=(shared_semaphore&&) = default;
  virtual ~shared_semaphore() = default;

  inline semaphore_result wait(int timeout_ms = -1);
  inline semaphore_result wait(std::chrono::milliseconds timeout);
  inline void post();
};

// inline implementations

shared_semaphore::shared_semaphore(int capacity) : impl_{new semaphore(capacity)} {
}

semaphore_result shared_semaphore::wait(int timeout) {
  return impl_->wait(timeout);
}

semaphore_result shared_semaphore::wait(std::chrono::milliseconds timeout) {
  return impl_->wait(timeout);
}

void shared_semaphore::post() {
  impl_->post();
}

}  // namespace boson

#endif  // BOSON_SEMAPHORE_H_
