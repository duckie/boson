#ifndef BOSON_THREAD_H_
#define BOSON_THREAD_H_
#pragma once

#include <list>
#include <memory>
#include <thread>
#include <vector>
#include <json_backbone.hpp>
#include <cassert>
#include "routine.h"
#include "queues/weakrb.h"
#include "event_loop.h"

namespace boson {

class engine;
using thread_id = std::size_t;

namespace context {

enum class thread_status {
  idle,       // Thread waits to be unlocked
  busy,       // Thread executes a routine
  finishing,  // Thread no longer executes a routine and is not required to wait
  finished    // Thread no longer executes a routine and is not required to wait
};

enum class thread_command_type {
  add_routine,
  finish
};

using thread_command_data = json_backbone::variant<std::nullptr_t, std::unique_ptr<routine>>;

struct thread_command {
  thread_command_type type;
  thread_command_data data;
};

/**
 * engine_proxy represents and engine view from the thread
 *
 * It encapsulates the semantics for the thread to identify
 * on the engine. currently, this semantics is an id. 
 */
class engine_proxy {
  // Use a pointer here to get free move ctor and operator
  engine* engine_;
  thread_id current_thread_id_;

 public:
  engine_proxy(engine& engine) 
    : engine_(&engine) 
  {}

  void set_id();
};


/**
 * Thread encapsulates an instance of an real thread
 *
 */
class thread : public event_handler {
  using routine_ptr_t = std::unique_ptr<routine>;
  using engine_queue_t = queues::weakrb<thread_command, 100>;

  engine_proxy engine_proxy_;
  std::vector<routine_ptr_t> scheduled_routines_;
  thread_status status_{thread_status::idle};
  //uv_loop_t uv_loop_;
  event_loop loop_;

  engine_queue_t engine_queue_;
  //uv_async_t engine_async_handle_;
  //uv_async_t self_handle_;
  int engine_event_id_;
  int self_event_id_;

  // Member function called by its matching C-Style callback
  void handle_engine_event() {
    thread_command received_command;
    while (engine_queue_.pop(received_command)) {
      switch (received_command.type) {
        case thread_command_type::add_routine:
          scheduled_routines_.emplace_back(received_command.data.template get<routine_ptr_t>().release());
          break;
        case thread_command_type::finish:
          status_ = thread_status::finishing;
          break;
      }
    }
    execute_scheduled_routines();
  }

  void close_handles() {
    loop_.unregister_event(engine_event_id_);
    loop_.unregister_event(self_event_id_);
  }

 public:
  thread(thread const&) = delete;
  thread(thread&&) = default;
  thread& operator=(thread const&) = delete;
  thread& operator=(thread&&) = default;


  // Event handler interface
  void event(int event_id, void* data) override {
    if (event_id == engine_event_id_) {
      handle_engine_event();
    }
    else if (event_id == self_event_id_)
      execute_scheduled_routines();
  }

  void read(int fd, void* data) override {
  }

  void write(int fd, void* data) override {
  }

  // callaed by engine
  bool push_command(thread_command& command) {
    return engine_queue_.push(command);
  };

  // called by engine
  void execute_commands() {
    loop_.send_event(engine_event_id_);
  }

  thread(engine& parent_engine) : engine_proxy_(parent_engine), loop_(*this) {
    engine_event_id_ = loop_.register_event(&engine_event_id_);
    self_event_id_ = loop_.register_event(&self_event_id_);
  }

  ~thread() {
  };

  void execute_scheduled_routines() {
    //while (!scheduled_routines_.empty()) {
    std::vector<routine_ptr_t> next_scheduled_routines;
    next_scheduled_routines.reserve(scheduled_routines_.size());
    for(auto& routine : scheduled_routines_) {
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
      close_handles();
      status_ = thread_status::finished;
    }
    else {
      // Re schedule a loop
      loop_.send_event(self_event_id_);
    }
  }

  void loop() {
    engine_proxy_.set_id();  // Tells the engine which thread id we got 
    //do {
      //switch (status_) {
        //case thread_status::idle:
          //{
          //status_ = thread_status::busy;
          //if (thread_command::finish == command) {
            //status_ = thread_status::finished;
          //}
          //}
          //break;
        //case thread_status::busy:
          //execute_scheduled_routines();
          //status_ = thread_status::idle;
          //break;
        //case thread_status::finished:
          //execute_scheduled_routines();
          //break;
      //}
    //} while(thread_status::finished != status_);
    while(status_ != thread_status::finished) {
      loop_.loop(1);
    }
  }

  inline void operator()() {
    loop();
  };
};

}  // namespace context
}  // namespace boson

#endif  // BOSON_THREAD_H_ 
