#ifndef BOSON_CHANNEL_H_
#define BOSON_CHANNEL_H_

#include <memory>
#include "internal/routine.h"
#include "queues/mpmc.h"
#include "internal/thread.h"

namespace boson {

/**
 * Semaphore for routines only
 *
 * The boson semaphore may only be used from routines.
 */
class semaphore {
  using routine_ptr_t = std::unique_ptr<internal::routine>;
  queues::unbounded_mpmc<routine_ptr_t> waiters_;
  std::atomic<int> counter_;

  /**
   * Tries to unlock a waiter
   *
   * This is defered to the thread maintaining said routine. So we might
   * be suspended then unlocked right after.
   */
  void pop_a_waiter();

 public:
  semaphore(int capacity);
  semaphore(semaphore const&) = delete;
  semaphore(semaphore&&) = default;
  semaphore& operator=(semaphore const&) = delete;
  semaphore& operator=(semaphore&&) = default;
  ~semaphore() = default;

  /** 
   * Takes a semaphore ticker if it could, otherwise suspend the routine until a ticker is available
   */
  void wait();

  /**
   * Give back semaphore ticker
   */
  void post();
};



}  // namespace boson


#endif  // BOSON_CHANNEL_H_

