#include "boson/semaphore.h"
#include <cassert>
#include "boson/engine.h"
#include "boson/logger.h"

namespace boson {

semaphore::semaphore(int capacity)
    : waiters_{static_cast<int>(internal::current_thread()->get_engine().max_nb_cores())},
      counter_{capacity} {
}

semaphore::~semaphore() {
}

bool semaphore::pop_a_waiter(internal::thread* current) {
  using namespace internal;
  routine* current_routine = nullptr;
  int result = 1;
  if(0 < result) {
    routine* waiter = static_cast<routine*>(waiters_.read(current->id()));
    if (waiter) {
      assert(waiter->thread_);
      thread* managing_thread = waiter->thread_;
      managing_thread->push_command(
          current->id(), std::make_unique<thread_command>(
                             thread_command_type::schedule_waiting_routine,
                             std::make_tuple(waiter->id(), static_cast<int>(waiter->status()),
                                             std::unique_ptr<routine>(waiter))));
      return true;
    }
  }
  return true;
}

void semaphore::wait() {
  using namespace internal;
  int result = counter_.fetch_sub(1,std::memory_order_acquire);
  routine* current_routine = nullptr;
  while (result <= 0) {
    // We failed to get the semaphore, we have to suspend the routine
    thread* this_thread = internal::current_thread();
    assert(this_thread);
    routine* current_routine = this_thread->running_routine();
    current_routine->status_ = routine_status::wait_sema_wait;
    this_thread->context() = jump_fcontext(this_thread->context().fctx, this);
    result = counter_.fetch_sub(1,std::memory_order_acquire);
    current_routine->previous_status_ = routine_status::wait_sema_wait;
    current_routine->status_ = routine_status::running;
  }
}

void semaphore::post() {
  using namespace internal;
  int result = counter_.fetch_add(1,std::memory_order_release);
  if (0 <= result) {
    // We may not gotten in the middle of a wait, so we cant avoid to try a pop
    pop_a_waiter(internal::current_thread());
  }
  // We do not yield, this is the wait purpose
}

}  // namespace boson
