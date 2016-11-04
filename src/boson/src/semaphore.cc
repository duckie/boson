#include "boson/semaphore.h"
#include <cassert>
#include "boson/engine.h"

using namespace std::chrono;

namespace boson {

semaphore::semaphore(int capacity)
    : waiters_{static_cast<int>(internal::current_thread()->get_engine().max_nb_cores()+1)},
      counter_{capacity} {
}

semaphore::~semaphore() {
  // Clean up stale events
  std::pair<internal::thread*,std::size_t>* waiter = nullptr;
  auto current_thread_id = internal::current_thread()->id();
  while((waiter = static_cast<decltype(waiter)>(waiters_.read(current_thread_id))))
    delete waiter;
}

bool semaphore::pop_a_waiter(internal::thread* current) {
  using namespace internal;
  routine* current_routine = nullptr;
  int result = 1;
  if(0 < result) {
    auto waiter = static_cast<std::pair<internal::thread*,std::size_t>*>(waiters_.read(current->id()));
    if (waiter) {
      thread* managing_thread = waiter->first;
      managing_thread->push_command(
          current->id(), std::make_unique<thread_command>(
                             thread_command_type::schedule_waiting_routine,
                             std::make_pair(this->shared_from_this(),waiter->second)));
      delete waiter;
      return true;
    }
  }
  return true;
}

void semaphore::disable() {
  using namespace internal;
  counter_.store(disabled_standpoint, std::memory_order_release);
  std::pair<thread*, std::size_t>* waiter = nullptr;
  auto current_thread_id = current_thread()->id();
  while ((waiter = static_cast<decltype(waiter)>(waiters_.read(current_thread_id)))) {
    thread* managing_thread = waiter->first;
    managing_thread->push_command(
        current_thread_id,
        std::make_unique<thread_command>(thread_command_type::schedule_waiting_routine,
                                         std::make_pair(this->shared_from_this(), waiter->second)));
    delete waiter;
  }
}

semaphore_result semaphore::wait(int timeout) {
  using namespace internal;
  int result = counter_.fetch_sub(1,std::memory_order_acquire);
  routine* current_routine = nullptr;
  event_type happened_type = event_type::sema_wait;
  if(result <= 0) {
    if (disabling_threshold < result) {
      counter_.fetch_add(1,std::memory_order_relaxed);
      return { semaphore_return_value::disabled };
    }
    // We failed to get the semaphore, we have to suspend the routine
    thread* this_thread = internal::current_thread();
    assert(this_thread);
    routine* current_routine = this_thread->running_routine();
    current_routine->start_event_round();
    current_routine->add_semaphore_wait(this);
    if (0 <= timeout) {
      current_routine->add_timer(
          time_point_cast<milliseconds>(high_resolution_clock::now() + milliseconds(timeout)));
    }
    current_routine->commit_event_round();
    happened_type = current_routine->happened_type_;
    current_routine->previous_status_ = routine_status::wait_events;
    current_routine->status_ = routine_status::running;
  }
  return {happened_type == event_type::sema_wait
              ? semaphore_return_value::ok
              : happened_type == event_type::sema_closed ? semaphore_return_value::disabled
                                                         : semaphore_return_value::timedout};
}

void semaphore::post() {
  using namespace internal;
  int result = counter_.fetch_add(1,std::memory_order_release);
  if (disabling_threshold < result) {
    counter_.fetch_sub(1,std::memory_order_relaxed);
  }
  else if(0 <= result) {
    // We may not gotten in the middle of a wait, so we cant avoid to try a pop
    pop_a_waiter(internal::current_thread());
  }
  // We do not yield, this is the wait purpose
}

}  // namespace boson
