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
          waiters_ = new queues::wfqueue<internal::routine*>(current->get_engine().max_nb_cores()+1);
        }
        mut_.unlock();
    }
    return waiters_;
  }
  return nullptr;
}

//semaphore::semaphore(int capacity) : waiters_{new queues::wfqueue<internal::routine*>(25)}, counter_{capacity} {
semaphore::semaphore(int capacity) : waiters_{nullptr}, counter_{capacity} {
  get_queue(internal::current_thread());
}

semaphore::~semaphore() {
  delete waiters_;
}

bool semaphore::pop_a_waiter(internal::routine* current_routine) {
  using namespace internal;
  thread* current = current_routine->thread_;
  routine* waiter = get_queue(current)->pop(current->id());
  if (waiter) {
    assert(waiter->thread_);
    thread* managing_thread = waiter->thread_;
    //boson::debug::log("Routine {}:{} to unlock itself", current->id(), current_routine->id(),
                      //managing_thread->id(), waiter->id());
    if (current_routine->id() == waiter->id()) {
      return false;
    } else {
      //boson::debug::log("Routine {}:{} to unlock {}:{}:{}", current->id(), current_routine->id(),
                        //managing_thread->id(), waiter->id(),static_cast<int>(waiter->status()));
      managing_thread->push_command(current->id(),
          std::make_unique<thread_command>(thread_command_type::schedule_waiting_routine, std::unique_ptr<routine>(waiter)));
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
    //init_queue(this_thread);
    routine* current_routine = this_thread->running_routine();
    //boson::debug::log("Routine {}:{}:{} failed to take the lock {}", this_thread->id(),
                      //current_routine->id(), static_cast<int>(current_routine->status()), reinterpret_cast<size_t>(this));
    current_routine->waiting_data_ = nullptr;
    current_routine->status_ = routine_status::wait_sema_wait;
    get_queue(this_thread)->push(this_thread->id(), current_routine);
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
      //boson::debug::log("Routine {}:{} suspended on wait", this_thread->id(),
                        //current_routine->id());
      this_thread->context() = jump_fcontext(this_thread->context().fctx, nullptr);
      //boson::debug::log("Routine {}:{} resumed on wait", this_thread->id(), current_routine->id());
      current_routine->previous_status_ = routine_status::wait_sema_wait;
    }
    current_routine->status_ = routine_status::running;
    result = counter_.fetch_sub(1, std::memory_order::memory_order_acquire);
  }
}

void semaphore::post() {
  using namespace internal;
  int result = counter_.fetch_add(1, std::memory_order::memory_order_acq_rel);
  if (0 <= result) {
    // We may not gotten in the middle of a wait, so we cant avoid to try a pop
    pop_a_waiter(internal::current_thread()->running_routine());
  }
  // We do not yield, this is the wait purpose
}

}  // namespace boson
