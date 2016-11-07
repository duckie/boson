#include "internal/thread.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include "engine.h"
#include "exception.h"
#include "internal/routine.h"
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
    nb_pending_commands_.fetch_sub(1);
    switch (received_command->type) {
      case thread_command_type::add_routine:
        scheduled_routines_.emplace_back(
                routine_slot{std::move(received_command->data.get<routine_ptr_t>()), 0});
        break;
      case thread_command_type::schedule_waiting_routine: {
        auto& data = received_command->data.get<std::pair<std::weak_ptr<semaphore>, std::size_t>>();
        auto& shared_routine = suspended_slots_[data.second];
        // If not previously invalidated by a timeout
        if (shared_routine.ptr) {
          shared_routine.ptr->get()->set_as_semaphore_event_candidate(shared_routine.event_index);
        }
        else {
          auto sema_pointer = data.first.lock();
          if (sema_pointer)
            sema_pointer->pop_a_waiter(this);
        }
        suspended_slots_.free(data.second);
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

timed_routines_set& thread::register_timer(routine_time_point const& date, routine_slot slot) {
  auto index = suspended_slots_.allocate();
  suspended_slots_[index] = slot;
  auto& current_set = timed_routines_[date];
  current_set.slots.emplace_back(index);
  ++current_set.nb_active;
  return current_set;
}

std::size_t thread::register_semaphore_wait(routine_slot slot) {
  auto index = suspended_slots_.allocate();
  suspended_slots_[index] = slot;
  ++nb_suspended_routines_;
  return index;
}

void thread::register_read(int fd, routine_slot slot) {
  int existing_read = -1;
  tie(existing_read, std::ignore) = loop_.get_events(fd);
  if (0 <= existing_read) {
    std::size_t slot_index = reinterpret_cast<std::size_t>(loop_.get_data(existing_read));
    suspended_slots_[slot_index] = slot;
  }
  else {
    auto index = suspended_slots_.allocate();
    suspended_slots_[index] = slot;
    loop_.register_read(fd, reinterpret_cast<void*>(index));
  }
  ++nb_suspended_routines_;
}

void thread::register_write(int fd, routine_slot slot) {
  int existing_write = -1;
  tie(std::ignore, existing_write) = loop_.get_events(fd);
  if (0 <= existing_write) {
    std::size_t slot_index = reinterpret_cast<std::size_t>(loop_.get_data(existing_write));
    suspended_slots_[slot_index] = slot;
  }
  else {
    auto index = suspended_slots_.allocate();
    suspended_slots_[index] = slot;
    loop_.register_write(fd, reinterpret_cast<void*>(index));
  }
  ++nb_suspended_routines_;
}

thread::thread(engine& parent_engine)
    : engine_proxy_(parent_engine),
      loop_(*this,static_cast<int>(parent_engine.max_nb_cores() + 1)),
      engine_queue_{static_cast<int>(parent_engine.max_nb_cores() + 1)} {
  engine_event_id_ = loop_.register_event(&engine_event_id_);
  engine_proxy_.set_id();  // Tells the engine which thread id we got
}

void thread::event(int event_id, void* data, event_status status) {
  if (event_id == engine_event_id_) {
    handle_engine_event();
  } 
}

void thread::read(int fd, void* data, event_status status) {
  auto& slot = suspended_slots_[reinterpret_cast<std::size_t>(data)];
  if (slot.ptr) {
    slot.ptr->get()->event_happened(slot.event_index, status);
  }
  else {
    // Dry run, just disable the event
    suspended_slots_.free(reinterpret_cast<std::size_t>(data));
    int existing_read = -1;
    tie(existing_read, std::ignore) = loop_.get_events(fd);
    if (0 <= existing_read)
      loop_.unregister(existing_read);
  }
}

void thread::write(int fd, void* data, event_status status) {
  auto& slot = suspended_slots_[reinterpret_cast<std::size_t>(data)];
  if (slot.ptr) {
    slot.ptr->get()->event_happened(slot.event_index, status);
  }
  else {
    // Dry run, just disable the event
    suspended_slots_.free(reinterpret_cast<std::size_t>(data));
    int existing_write= -1;
    tie(std::ignore, existing_write) = loop_.get_events(fd);
    if (0 <= existing_write)
      loop_.unregister(existing_write);
  }
}

// called by engine
void thread::push_command(thread_id from, std::unique_ptr<thread_command> command) {
  nb_pending_commands_.fetch_add(1);
  engine_queue_.write(from, command.release());
  loop_.send_event(engine_event_id_);
};

bool thread::execute_scheduled_routines() {
  decltype(scheduled_routines_) next_scheduled_routines;
  std::deque<std::tuple<size_t, routine_ptr_t>> new_timed_routines_;
  while (!scheduled_routines_.empty()) {
    // For now; we schedule them in order
    auto& slot = scheduled_routines_.front();
    if (slot.ptr) {
      auto routine = running_routine_ = slot.ptr->get();

      bool run_routine = true;
      // Try to get a semaphore ticket, if relevant
      if (routine->status() == routine_status::sema_event_candidate) {
        run_routine = routine->event_happened(slot.event_index);
        // If success, get back the unique ownserhip of the routine
        if (run_routine) {
          slot.ptr = routine_local_ptr_t(routine_ptr_t(routine));
        }
      }

      if (run_routine) routine->resume(this);
      switch (routine->status()) {
        case routine_status::is_new:
        case routine_status::running: {
          // Not supposed to happen
          assert(false);
        } break;
        case routine_status::yielding: {
          // If not finished, then we reschedule it
          next_scheduled_routines.emplace_back(
              routine_slot{routine_local_ptr_t(routine_ptr_t(slot.ptr->release())), 0});
        } break;
        case routine_status::wait_events: {
          slot.ptr->release();
        } break;
        case routine_status::sema_event_candidate: {
          // Thats means no event happened for the routine, so we must let the slot pointer
          // untouched for other events to stay valid
          routine->status_ = routine_status::wait_events;
        } break;
        case routine_status::finished: {
          // Should have been made by the routine by closing the FD
        } break;
      };

      // if (routine.get()) {
      // debug::log("Routine {}:{}:{} will be deleted.", id(), routine->id(),
      // static_cast<int>(routine->status()));
      //}
    }
    scheduled_routines_.pop_front();
  }

  // Yielded routines are immediately scheduled
  scheduled_routines_ = std::move(next_scheduled_routines);

  // Cleanup canceled timers
  auto first_timed_routines = begin(timed_routines_);
  while (first_timed_routines != end(timed_routines_) && first_timed_routines->second.nb_active == 0) {
    // Clean up suspended vector
    for (auto index : first_timed_routines->second.slots)
      suspended_slots_.free(index);
    // Delete full record
    first_timed_routines = timed_routines_.erase(first_timed_routines);
  }

  // If finished and no more routines, exit
  size_t nb_pending_commands = nb_pending_commands_;
  bool no_more_routines =
      scheduled_routines_.empty() && timed_routines_.empty() && 0 == nb_suspended_routines_;
  if (no_more_routines) {
    if (0 == nb_pending_commands) {
        if (thread_status::finishing == status_) {
          unregister_all_events();
          status_ = thread_status::finished;
          return false;
        }
        else {
          engine_proxy_.notify_idle(0);
          return false;
        }
    }
  } else {
    if (scheduled_routines_.empty()) {
      size_t nb_routines = timed_routines_.size() + nb_suspended_routines_;
      if (0 == nb_pending_commands) {
        if (0 == nb_routines) {
            engine_proxy_.notify_idle(0);
        }
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


    // Compute next timeout
    bool fire_timed_out_routines = false;
    auto first_timed_routines = begin(timed_routines_);
    if (0 != timeout_ms) {
      if (first_timed_routines != end(timed_routines_)) {
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
      for (auto& timed_routine : first_timed_routines->second.slots) {
        auto& slot =  suspended_slots_[timed_routine];
        if (slot.ptr)
          slot.ptr->get()->event_happened(slot.event_index);
        suspended_slots_.free(timed_routine);
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
