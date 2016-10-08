#include "boson/semaphore.h"
#include "boson/engine.h"
#include <cassert>
#include "boson/logger.h"

namespace boson {

auto semaphore::get_queue(internal::thread* current) -> queue_t* {
  if (current) {
    if (!waiters_) {
        mut_.lock();
        if (!waiters_) {
          waiters_ = new queue_t(current->get_engine().max_nb_cores()*2);
        }
        mut_.unlock();
    }
    return waiters_;
  }
  return nullptr;
}

semaphore::semaphore(int capacity) : waiters_{nullptr}, counter_{capacity} {
  get_queue(internal::current_thread());
}

semaphore::~semaphore() {
  delete waiters_;
}

bool semaphore::pop_a_waiter(internal::thread* current) {
  using namespace internal;
  routine* waiter = static_cast<routine*>(get_queue(current)->pop(current->id()));
  if (waiter) {
    assert(waiter->thread_);
    thread* managing_thread = waiter->thread_;
      managing_thread->push_command(current->id(),
          std::make_unique<thread_command>(thread_command_type::schedule_waiting_routine, std::make_tuple(waiter->id(), static_cast<int>(waiter->status()), std::unique_ptr<routine>(waiter))));
      return true;
  }
  return true;
}

void semaphore::wait() {
  using namespace internal;
  int result = counter_.fetch_sub(1, std::memory_order::memory_order_acquire);
  //int result = counter_.fetch_sub(1);
  routine* current_routine = nullptr;
  while (result <= 0) {
    // We failed to get the semaphore, we have to suspend the routine
    thread* this_thread = internal::current_thread();
    assert(this_thread);
    routine* current_routine = this_thread->running_routine();
    current_routine->waiting_data_ = nullptr;
    current_routine->status_ = routine_status::wait_sema_wait;
    this_thread->context() = jump_fcontext(this_thread->context().fctx, this);
    result = counter_.fetch_sub(1, std::memory_order::memory_order_acquire);
    current_routine->previous_status_ = routine_status::wait_sema_wait;
    current_routine->status_ = routine_status::running;
  }
}

void semaphore::post() {
  using namespace internal;
  int result = counter_.fetch_add(1, std::memory_order::memory_order_acq_rel);
  if (0 <= result) {
     //We may not gotten in the middle of a wait, so we cant avoid to try a pop
      pop_a_waiter(internal::current_thread());
  }
  // We do not yield, this is the wait purpose
}

}  // namespace boson
