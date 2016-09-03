#include "thread.h"
#include "engine.h"
#include "routine.h"

namespace boson {
namespace context {

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
      case thread_command_type::finish:
        status_ = thread_status::finishing;
        break;
    }
  }
  execute_scheduled_routines();
}

void thread::unregister_all_events() {
  loop_.unregister_event(engine_event_id_);
  loop_.unregister_event(self_event_id_);
}

thread::thread(engine& parent_engine) : engine_proxy_(parent_engine), loop_(*this) {
  engine_event_id_ = loop_.register_event(&engine_event_id_);
  self_event_id_ = loop_.register_event(&self_event_id_);
}

void thread::event(int event_id, void* data) {
  if (event_id == engine_event_id_) {
    handle_engine_event();
  } else if (event_id == self_event_id_)
    execute_scheduled_routines();
}

void thread::read(int fd, void* data) {
}

void thread::write(int fd, void* data) {
}

// callaed by engine
bool thread::push_command(thread_command& command) {
  return engine_queue_.push(command);
};

// called by engine
void thread::execute_commands() {
  loop_.send_event(engine_event_id_);
}

void thread::execute_scheduled_routines() {
  // while (!scheduled_routines_.empty()) {
  std::vector<routine_ptr_t> next_scheduled_routines;
  next_scheduled_routines.reserve(scheduled_routines_.size());
  for (auto& routine : scheduled_routines_) {
    // For now; we schedule them in order
    routine->resume();
    if (routine_status::finished != routine->status()) {
      // If not finished, then we reschedule it
      next_scheduled_routines.emplace_back(routine.release());
    }
  }
  scheduled_routines_ = std::move(next_scheduled_routines);

  // If finished and no more routines, exit
  if (0 == scheduled_routines_.size() && thread_status::finishing == status_) {
    unregister_all_events();
    status_ = thread_status::finished;
  } else {
    // Re schedule a loop
    loop_.send_event(self_event_id_);
  }
}

void thread::loop() {
  engine_proxy_.set_id();  // Tells the engine which thread id we got
  // do {
  // switch (status_) {
  // case thread_status::idle:
  //{
  // status_ = thread_status::busy;
  // if (thread_command::finish == command) {
  // status_ = thread_status::finished;
  //}
  //}
  // break;
  // case thread_status::busy:
  // execute_scheduled_routines();
  // status_ = thread_status::idle;
  // break;
  // case thread_status::finished:
  // execute_scheduled_routines();
  // break;
  //}
  //} while(thread_status::finished != status_);
  while (status_ != thread_status::finished) {
    loop_.loop(1);
  }
}
}
}  // namespace boson
