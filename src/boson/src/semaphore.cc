#include "boson/semaphore.h"
#include <cassert>
#include <iostream>

namespace boson {

semaphore::semaphore(int capacity) : waiters_{100}, counter_{capacity} {
  assert(0 < capacity);
}

bool semaphore::pop_a_waiter(internal::thread* current) {
  using namespace internal;
  std::unique_ptr<routine> waiter = nullptr;
  if (waiters_.pop(waiter) && waiter) {
    assert(waiter->thread_);
    thread* managing_thread = waiter->thread_;
    if (current == managing_thread) {
      waiter.release();
      return false;
    }
    else {
      managing_thread->push_command(
          {thread_command_type::schedule_waiting_routine, std::move(waiter)});
      return true;
    }
  }
  return false;
}

void semaphore::wait() {
  using namespace internal;
  //std::cout << "Wait\n" << std::flush;
  int result = counter_.fetch_sub(1, std::memory_order::memory_order_acquire);
  while (result < 0) {
    // We failed to get the semaphore, we have to suspend the routine
    thread* this_thread = internal::current_thread();
    assert(this_thread);
    routine* current_routine = this_thread->running_routine();
    current_routine->waiting_data_ = nullptr;
    current_routine->status_ = routine_status::wait_sema_suspend;
    std::cout << "Fail " << this_thread->id() << std::flush;
    waiters_.push(current_routine);
    // We give back the spuriously taken ticket
    result = counter_.fetch_add(std::memory_order::memory_order_release);
    bool suspend = true;
    if (0 < result) {
      // A post happened while we were working, defer a pop then
      // I may pop myself here
      suspend = pop_a_waiter(this_thread);
    }
    if (suspend) {
      // Jump to main context
      std::cout << "Suspends " << this_thread->id() << std::endl;
      this_thread->context() = jump_fcontext(this_thread->context().fctx, nullptr);
      std::cout << "Resumed " << this_thread->id() << std::endl;
      current_routine->previous_status_ = routine_status::wait_sema_wait;
      current_routine->status_ = routine_status::running;
    }
    result = counter_.fetch_sub(1, std::memory_order::memory_order_acquire);
  }
}

void semaphore::post() {
  //std::cout << "Post\n" << std::flush;
  using namespace internal;
  int result = counter_.fetch_add(1, std::memory_order::memory_order_acq_rel);
  if (0 < result) {
    // We may not gotten in the middle of a wait, so we cant avoid to try a pop
    pop_a_waiter();
  }
  // We do not yield, this is the wait purpose
}

}  // namespace boson
