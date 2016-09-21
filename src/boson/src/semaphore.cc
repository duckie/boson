#include "boson/semaphore.h"
#include <cassert>

namespace boson {

semaphore::semaphore(int capacity) : waiters_{1}, counter_{capacity} {
  assert(0 < capacity);
}

void semaphore::pop_a_waiter() {}

void semaphore::wait() {
  using namespace internal;
  int result = counter_.fetch_sub(1, std::memory_order::memory_order_acquire);
  if (result < 0) {
    // We failed to get the semaphore, we have to suspend the routine
    thread* this_thread = current_thread;
    routine* current_routine = current_thread->running_routine();
    current_routine->waiting_data_ = nullptr;
    current_routine->status_ = routine_status::wait_sema_wait;
    waiters_.push(current_routine);
    // We give back the spuriously taken ticket
    result = counter_.fetch_add(std::memory_order::memory_order_release);
    if (0 < result) {
      // A post happened while we were working, defer a pop then
      pop_a_waiter();
    }
    // Jump to main context
    this_thread->context() = jump_fcontext(this_thread->context().fctx, nullptr);
    current_routine->previous_status_ = routine_status::wait_sema_wait;
    current_routine->status_ = routine_status::running;
  }
}

void semaphore::post() {
  using namespace internal;
  int result = counter_.fetch_add(1, std::memory_order::memory_order_acq_rel);
  if (0 < result) {
    // We may not gotten in the middle of a wait, so we cant avoid to try a pop
    pop_a_waiter();
  }
  // We do not yield, this is the wait purpose
}


}  // namespace boson
