#include "internal/thread.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include "engine.h"
#include "exception.h"
#include "fmt/format.h"
#include "internal/routine.h"
#include "logger.h"
#include "logger.h"
#include "semaphore.h"

namespace boson {
namespace internal {

// class engine_proxy;

engine_proxy::engine_proxy(engine& parent_engine) : engine_(&parent_engine) {
}


engine_proxy::~engine_proxy() {
}

void engine_proxy::notify_end() {
  engine_->push_command(current_thread_id_,
                        std::make_unique<engine::command>(
                            current_thread_id_, engine::command_type::notify_end_of_thread,
                            engine::command_data{nullptr}));
}

routine_id engine_proxy::get_new_routine_id() {
  return engine_->current_routine_id_++;
}

void engine_proxy::notify_idle(size_t nb_suspended_routines) {
  engine_->push_command(
      current_thread_id_,
      std::make_unique<engine::command>(current_thread_id_, engine::command_type::notify_idle,
                                        engine::command_data{nb_suspended_routines}));
}

void engine_proxy::start_routine(std::unique_ptr<routine> new_routine) {
  start_routine(engine_->max_nb_cores(), std::move(new_routine));
}

void engine_proxy::start_routine(thread_id target_thread, std::unique_ptr<routine> new_routine) {
  engine_->push_command(current_thread_id_, std::make_unique<engine::command>(
                                                target_thread, engine::command_type::add_routine,
                                                engine::command_new_routine_data{
                                                    target_thread, std::move(new_routine)}));
}

void engine_proxy::fd_panic(int fd) {
  engine_->push_command(
      current_thread_id_,
      std::make_unique<engine::command>(current_thread_id_, engine::command_type::fd_panic, fd));
}

void engine_proxy::set_id() {
  current_thread_id_ = engine_->register_thread_id();
}

void thread::handle_engine_event() {
  thread_command* received_command = nullptr;
  while ((received_command = static_cast<thread_command*>(engine_queue_.read(id())))) {
    static int sub = 0;
    // nb_pending_commands_.fetch_sub(1,std::memory_order_release);
    nb_pending_commands_.fetch_sub(1);
    switch (received_command->type) {
      case thread_command_type::add_routine:
        scheduled_routines_.emplace_back(
            received_command->data.get<routine_ptr_t>().release());
        break;
      case thread_command_type::schedule_waiting_routine: {
        auto& t = received_command->data.get<std::tuple<int, int, routine_ptr_t>>();
        int rid = std::get<0>(t);
        int status = std::get<1>(t);
        routine* current_routine = std::get<2>(t).release();
        assert(current_routine->status() == routine_status::wait_sema_wait);
        current_routine->expected_event_happened();
        --suspended_routines_;
        scheduled_routines_.emplace_back(current_routine);
      } break;
      case thread_command_type::finish:
        status_ = thread_status::finishing;
        break;
      case thread_command_type::fd_panic:
        auto& fd = received_command->data.get<int>();
        loop_.send_fd_panic(id(),fd);
        break;
    }
    delete received_command;
  }
}

void thread::unregister_all_events() {
  loop_.unregister(engine_event_id_);
  //loop_.unregister(self_event_id_);
}

thread::thread(engine& parent_engine)
    : engine_proxy_(parent_engine),
      loop_(*this,static_cast<int>(parent_engine.max_nb_cores() + 1)),
      engine_queue_{static_cast<int>(parent_engine.max_nb_cores() + 1)} {
  engine_event_id_ = loop_.register_event(&engine_event_id_);
  //self_event_id_ = loop_.register_event(&self_event_id_);
  engine_proxy_.set_id();  // Tells the engine which thread id we got
}

void thread::event(int event_id, void* data, event_status status) {
  if (event_id == engine_event_id_) {
    handle_engine_event();
  } else if (event_id == self_event_id_) {
    // execute_scheduled_routines();
  }
}

void thread::read(int fd, void* data, event_status status) {
  routine* target_routine = static_cast<routine*>(data);
  target_routine->expected_event_happened();
  if (status == event_status::panic) {
    target_routine->waiting_data().get<routine_io_event>().panic = true;
  }
  --suspended_routines_;
  scheduled_routines_.emplace_back(target_routine);
}

void thread::write(int fd, void* data, event_status status) {
  routine* target_routine = static_cast<routine*>(data);
  target_routine->expected_event_happened();
  if (status == event_status::panic) {
    target_routine->waiting_data().get<routine_io_event>().panic = true;
  }
  --suspended_routines_;
  scheduled_routines_.emplace_back(target_routine);
}

// called by engine
void thread::push_command(thread_id from, std::unique_ptr<thread_command> command) {
  nb_pending_commands_.fetch_add(1);
  engine_queue_.write(from, command.release());
  loop_.send_event(engine_event_id_);
};

// called by engine
// void thread::execute_commands() {
//}

namespace {
inline void clear_previous_io_event(routine& routine, event_loop& loop) {
  if (routine.previous_status_is_io_block()) {
    routine_io_event& target_event = routine.waiting_data().get<routine_io_event>();
    if (0 <= target_event.event_id) {
      loop.unregister(target_event.event_id);
    }
  }
}
}

bool thread::execute_scheduled_routines() {
  decltype(scheduled_routines_) next_scheduled_routines;
  std::list<std::tuple<size_t, routine_ptr_t>> new_timed_routines_;
  while (!scheduled_routines_.empty()) {
    // For now; we schedule them in order
    auto& routine = scheduled_routines_.front();
    running_routine_ = routine.get();
    routine->resume(this);
    switch (routine->status()) {
      case routine_status::is_new: {
        // Not supposed to happen
        assert(false);
      } break;
      case routine_status::running: {
        // Not supposed to happen
        assert(false);
      } break;
      case routine_status::yielding: {
        // If not finished, then we reschedule it
        clear_previous_io_event(*routine, loop_);
        next_scheduled_routines.emplace_back(routine.release());
      } break;
      case routine_status::wait_timer: {
        clear_previous_io_event(*routine, loop_);
        auto target_data = routine->waiting_data().raw<routine_time_point>();
        timed_routines_[target_data].emplace_back(routine.release());
      } break;
      case routine_status::wait_sys_read: {
        routine_io_event& target_event = routine->waiting_data().get<routine_io_event>();
        ++suspended_routines_;
        if (!target_event.is_same_as_previous_event) {
          clear_previous_io_event(*routine, loop_);
          target_event.event_id = loop_.register_read(target_event.fd, routine.release());
        } else {
          routine.release();
        }
      } break;
      case routine_status::wait_sys_write: {
        routine_io_event& target_event = routine->waiting_data().get<routine_io_event>();
        ++suspended_routines_;
        if (!target_event.is_same_as_previous_event) {
          clear_previous_io_event(*routine, loop_);
          target_event.event_id = loop_.register_write(target_event.fd, routine.release());
        } else {
          routine.release();
        }
      } break;
      case routine_status::wait_sema_wait: {
        clear_previous_io_event(*routine, loop_);
        semaphore* missed_semaphore = static_cast<semaphore*>(routine->context_.data);
        missed_semaphore->waiters_.write(id(), routine.release());
        int result = missed_semaphore->counter_.fetch_add(1,std::memory_order_release);
        if (0 <= result) {
          missed_semaphore->pop_a_waiter(this);
        }
        ++suspended_routines_;
      } break;
      case routine_status::finished: {
        clear_previous_io_event(*routine, loop_);
        // Should have been made by the routine by closing the FD
      } break;
    };

    // if (routine.get()) {
    // debug::log("Routine {}:{}:{} will be deleted.", id(), routine->id(),
    // static_cast<int>(routine->status()));
    //}
    scheduled_routines_.pop_front();
  }

  // Yielded routines are immediately scheduled
  scheduled_routines_ = std::move(next_scheduled_routines);

  // Timed routines are added in the timer list
  // This is made in two step to limit the syscall to get the current date

  // If finished and no more routines, exit
  size_t nb_pending_commands = nb_pending_commands_;
  bool no_more_routines =
      scheduled_routines_.empty() && timed_routines_.empty() && 0 == suspended_routines_;
  if (no_more_routines) {
    if (thread_status::finishing == status_) {
      unregister_all_events();
      status_ = thread_status::finished;
      return false;
    } else if (0 == nb_pending_commands) {
      engine_proxy_.notify_idle(0);
      return false;
    }
  } else {
    if (scheduled_routines_.empty()) {
      size_t nb_routines = timed_routines_.size() + suspended_routines_;
      if (0 == nb_pending_commands) {
        if (0 == nb_routines) engine_proxy_.notify_idle(0);
        return false;
      } else {
        // Schedule pending commands immediately
        loop_.send_event(engine_event_id_);
        return true;
      }
    } else {
      // If some routines already are scheduled, then throw an event to force a loop execution
      return true;
    }
  }
  return true;
}

void thread::loop() {
  using namespace std::chrono;
  current_thread() = this;

  // Check if we should have a time out
  int timeout_ms = -1;
  while (status_ != thread_status::finished) {
    auto first_timed_routines = begin(timed_routines_);
    bool fire_timed_out_routines = false;
    if (0 != timeout_ms) {
      if (!timed_routines_.empty()) {
        // Compute next timeout
        timeout_ms =
            duration_cast<milliseconds>(first_timed_routines->first - high_resolution_clock::now())
                .count();
        if (timeout_ms < 0)
          timeout_ms = 0;
        if (timeout_ms == 0)
          fire_timed_out_routines = true;
      }
    }
    auto return_code = loop_.loop(1, timeout_ms);
    switch (return_code) {
      case loop_end_reason::max_iter_reached:
        break;
      case loop_end_reason::timed_out:
        fire_timed_out_routines = true;
        break;
      case loop_end_reason::error_occured:
      default:
        throw exception("Boson unknown error");
        return;
    }
    if (fire_timed_out_routines) {
      // Schedule routines that timed out
      for (auto& timed_routine : first_timed_routines->second) {
        timed_routine->expected_event_happened();
        scheduled_routines_.emplace_back(timed_routine.release());
      }
      timed_routines_.erase(first_timed_routines);
    }
    timeout_ms = execute_scheduled_routines() ? 0 : -1;
  }

  engine_proxy_.notify_end();
  // Should not be useful, but a lil discipline does not hurt
  // current_thread = nullptr;
}

thread*& current_thread() {
  thread_local thread* cur_thread = nullptr;
  return cur_thread;
}

}  // namespace internal
}  // namespace boson
