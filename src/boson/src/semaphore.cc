#include "boson/semaphore.h"
#include <cassert>
#include "boson/logger.h"

namespace boson {

semaphore::semaphore(int capacity) : waiters_{5000}, counter_{capacity} {
  //assert(0 < capacity);
}

bool semaphore::pop_a_waiter(internal::routine* current_routine) {
  using namespace internal;
  thread* current = current_routine->thread_;
  std::unique_ptr<routine> waiter = nullptr;
  if (waiters_.pop(waiter) && waiter) {
    assert(waiter->thread_);
    thread* managing_thread = waiter->thread_;
    //boson::debug::log("Routine {}:{} to unlock itself", current->id(), current_routine->id(),
                      //managing_thread->id(), waiter->id());
    if (current_routine->id() == waiter->id()) {
      waiter.release();
      return false;
    } else {
      //boson::debug::log("Routine {}:{} to unlock {}:{}", current->id(), current_routine->id(),
                        //managing_thread->id(), waiter->id());
      managing_thread->push_command(
          {thread_command_type::schedule_waiting_routine, std::move(waiter)});
      return true;
    }
  }
  return true;
}

void semaphore::wait() {
  using namespace internal;
  int result = counter_.fetch_sub(1, std::memory_order::memory_order_acquire);
  //int result = counter_.fetch_sub(1);
  while (result <= 0) {
    // We failed to get the semaphore, we have to suspend the routine
    thread* this_thread = internal::current_thread();
    assert(this_thread);
    routine* current_routine = this_thread->running_routine();
    //boson::debug::log("Routine {}:{} failed to take the lock {}", this_thread->id(),
                      //current_routine->id(), reinterpret_cast<size_t>(this));
    current_routine->waiting_data_ = nullptr;
    bool pushed = waiters_.push(current_routine);
    assert(pushed);
    // We give back the spuriously taken ticket
     result = counter_.fetch_add(1, std::memory_order::memory_order_release);
    //result = counter_.fetch_add(1);
    bool suspend = true;
    if (0 <= result) {
      // A post happened while we were working, defer a pop then
      // I may pop myself here
      suspend = pop_a_waiter(current_routine);
    }
    if (suspend) {
      // Jump to main context
      current_routine->status_ = routine_status::wait_sema_wait;
      //boson::debug::log("Routine {}:{} suspended on wait", this_thread->id(),
                        //current_routine->id());
      this_thread->context() = jump_fcontext(this_thread->context().fctx, nullptr);
      //boson::debug::log("Routine {}:{} resumed on wait", this_thread->id(), current_routine->id());
      current_routine->previous_status_ = routine_status::wait_sema_wait;
      current_routine->status_ = routine_status::running;
    }
    result = counter_.fetch_sub(1, std::memory_order::memory_order_acquire);
    //result = counter_.fetch_sub(1);
  }
}

void semaphore::post() {
  using namespace internal;
  int result = counter_.fetch_add(1, std::memory_order::memory_order_acq_rel);
  //int result = counter_.fetch_add(1);
  if (0 <= result) {
    // We may not gotten in the middle of a wait, so we cant avoid to try a pop
    pop_a_waiter(internal::current_thread()->running_routine());
  }
  // We do not yield, this is the wait purpose
}

}  // namespace boson
