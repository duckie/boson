#ifndef BOSON_SEMAPHORE_H_
#define BOSON_SEMAPHORE_H_

#include <memory>
#include <chrono>
#include "internal/routine.h"
#include "internal/thread.h"
#include "queues/lcrq.h"

namespace boson {

/**
 * Semaphore for routines only
 *
 * The boson semaphore may only be used from routines.
 */
class semaphore {
  friend class internal::thread;
  friend class internal::routine;
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
   * takes a semaphore ticker if it could, otherwise suspend the routine until a ticker is available
   */
  bool wait(int timeout_ms = -1);

  inline bool wait(std::chrono::milliseconds);
  /**
   * give back semaphore ticket. Always non blocking
   */
  void post();
};


bool semaphore::wait(std::chrono::milliseconds timeout) {
  return wait(timeout.count());
}

/**
 * shared_semaphore is a wrapper for shared_ptr of a semaphore
 */
class shared_semaphore {
  std::shared_ptr<semaphore> impl_;

 public:
  inline shared_semaphore(int capacity);
  shared_semaphore(shared_semaphore const&) = default;
  shared_semaphore(shared_semaphore&&) = default;
  shared_semaphore& operator=(shared_semaphore const&) = default;
  shared_semaphore& operator=(shared_semaphore&&) = default;
  virtual ~shared_semaphore() = default;

  inline bool wait(int timeout_ms = -1);
  inline bool wait(std::chrono::milliseconds timeout);
  inline void post();
};

// inline implementations

shared_semaphore::shared_semaphore(int capacity) : impl_{new semaphore(capacity)} {
}

bool shared_semaphore::wait(int timeout) {
  return impl_->wait(timeout);
}

bool shared_semaphore::wait(std::chrono::milliseconds timeout) {
  return impl_->wait(timeout);
}


void shared_semaphore::post() {
  impl_->post();
}

}  // namespace boson

#endif  // BOSON_SEMAPHORE_H_
