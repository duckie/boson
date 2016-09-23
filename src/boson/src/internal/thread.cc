#include "internal/thread.h"
#include <cassert>
#include <chrono>
#include "engine.h"
#include "exception.h"
#include "internal/routine.h"
#include <iostream>

namespace boson {
namespace internal {

// class engine_proxy;

engine_proxy::engine_proxy(engine& parent_engine) : engine_(&parent_engine) {
}

void engine_proxy::set_id() {
  current_thread_id_ = engine_->register_thread_id();
}

// class thread;

void thread::handle_engine_event() {
  thread_command received_command;
  while (engine_queue_.pop(received_command)) {
    switch (received_command.type) {
      case thread_command_type::add_routine:
        scheduled_routines_.emplace_back(
            received_command.data.template get<routine_ptr_t>().release());
        break;
      case thread_command_type::schedule_waiting_routine:
        std::cout << "Yay schedule me " <<received_command.data.template get<routine_ptr_t>().get() << std::endl;
        --suspended_routines_;
        scheduled_routines_.emplace_back(
            received_command.data.template get<routine_ptr_t>().release());
        break;
      case thread_command_type::finish:
        status_ = thread_status::finishing;
        break;
    }
  }
  // execute_scheduled_routines();
}

void thread::unregister_all_events() {
  loop_.unregister(engine_event_id_);
  loop_.unregister(self_event_id_);
}

thread::thread(engine& parent_engine) : engine_proxy_(parent_engine), loop_(*this) {
  engine_event_id_ = loop_.register_event(&engine_event_id_);
  self_event_id_ = loop_.register_event(&self_event_id_);
}

void thread::event(int event_id, void* data) {
  if (event_id == engine_event_id_) {
    handle_engine_event();
  } else if (event_id == self_event_id_) {
    // execute_scheduled_routines();
  }
}

void thread::read(int fd, void* data) {
  routine* target_routine = static_cast<routine*>(data);
  target_routine->expected_event_happened();
  --suspended_routines_;
  scheduled_routines_.emplace_back(target_routine);
}

void thread::write(int fd, void* data) {
  routine* target_routine = static_cast<routine*>(data);
  target_routine->expected_event_happened();
  --suspended_routines_;
  scheduled_routines_.emplace_back(target_routine);
}

// called by engine
void thread::push_command(thread_command&& command) {
  engine_queue_.push(std::move(command));
  loop_.send_event(engine_event_id_);
};

// called by engine
// void thread::execute_commands() {
//}

namespace {
inline void clear_previous_io_event(routine& routine, event_loop& loop) {
  if (routine.previous_status_is_io_block()) {
    routine_io_event& target_event = routine.waiting_data().raw<routine_io_event>();
    if (0 <= target_event.event_id) loop.unregister(target_event.event_id);
  }
}
}

void thread::execute_scheduled_routines() {
  // while (!scheduled_routines_.empty()) {
  decltype(scheduled_routines_) next_scheduled_routines;
  std::list<std::tuple<size_t, routine_ptr_t>> new_timed_routines_;
  while (!scheduled_routines_.empty()) {
    // For now; we schedule them in order
    auto& routine = scheduled_routines_.front();
    running_routine_ = routine.get();
    std::cout << "Resumes " << running_routine_ << std::endl;
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
        }
      } break;
      case routine_status::wait_sys_write: {
        routine_io_event& target_event = routine->waiting_data().get<routine_io_event>();
        ++suspended_routines_;
        if (!target_event.is_same_as_previous_event) {
          clear_previous_io_event(*routine, loop_);
          target_event.event_id = loop_.register_write(target_event.fd, routine.release());
        }
      } break;
      case routine_status::wait_sema_wait: {
        clear_previous_io_event(*routine, loop_);
        routine.release();
        ++suspended_routines_;
      } break;
      case routine_status::finished: {
        clear_previous_io_event(*routine, loop_);
      } break;
    };

    scheduled_routines_.pop_front();
  }

  // Yielded routines are immediately scheduled
  scheduled_routines_ = std::move(next_scheduled_routines);

  // Timed routines are added in the timer list
  // This is made in two step to limit the syscall to get the current date

  // If finished and no more routines, exit
  bool no_more_routines =
      scheduled_routines_.empty() && timed_routines_.empty() && 0 == suspended_routines_;
  if (no_more_routines && thread_status::finishing == status_) {
    unregister_all_events();
    status_ = thread_status::finished;
  } else {
    // If some routines already are scheduled, then throw an event to force a loop execution
    if (!scheduled_routines_.empty()) loop_.send_event(self_event_id_);
  }
}

void thread::loop() {
  using namespace std::chrono;
  engine_proxy_.set_id();  // Tells the engine which thread id we got
  current_thread() = this;

  while (status_ != thread_status::finished) {
    // Check if we should have a time out
    int timeout_ms = -1;
    auto first_timed_routines = begin(timed_routines_);
    if (!timed_routines_.empty()) {
      // Compute next timeout
      timeout_ms =
          duration_cast<milliseconds>(first_timed_routines->first - high_resolution_clock::now())
              .count();
    }
    auto return_code = loop_.loop(1, timeout_ms);
    switch (return_code) {
      case loop_end_reason::max_iter_reached:
        break;
      case loop_end_reason::timed_out:
        // Shcedule routines that timed out
        for (auto& timed_routine : first_timed_routines->second) {
          timed_routine->expected_event_happened();
          scheduled_routines_.emplace_back(timed_routine.release());
        }
        timed_routines_.erase(first_timed_routines);
        break;
      case loop_end_reason::error_occured:
      default:
        throw exception("Boson unknown error");
        return;
    }
    std::cout << "Come on jojette" << std::endl;
    execute_scheduled_routines();
  }

  // Should not be useful, but a lil discipline does not hurt
  // current_thread = nullptr;
}

thread*& current_thread() {
  thread_local thread* cur_thread = nullptr;
  return cur_thread;
}

}  // namespace internal
}  // namespace boson
