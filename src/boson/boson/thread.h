#ifndef BOSON_THREAD_H_
#define BOSON_THREAD_H_
#pragma once

#include <list>
#include <memory>
#include <thread>
#include <uv.h>
#include <iostream>
#include <json_backbone.hpp>
#include <cassert>
#include "routine.h"
#include "queues/weakrb.h"

namespace boson {

template <class StackTraits> class engine;

namespace context {

enum class thread_status {
  idle,     // Thread waits to be unlocked
  busy,     // Thread executes a routine
  finished  // Thread no longer executes a routine and is not required to wait
};

enum class thread_command_type {
  add_routine,
  finish
};

template <class StackTraits>
using thread_command_data = json_backbone::variant<std::nullptr_t, std::unique_ptr<routine<StackTraits>>>;

template <class StackTraits>
struct thread_command {
  thread_command_type type;
  thread_command_data<StackTraits> data;
};

/**
 * engine_proxy represents and engine view from the thread
 *
 * It encapsulates the semantics for the thread to identify
 * on the engine. currently, this semantics is an id. 
 */
template <class StackTraits> 
class engine_proxy {
  using engine_t = engine<StackTraits>;

  // Use a pointer here to get free move ctor and operator
  engine_t* engine_;
  size_t current_thread_id_;

 public:
  engine_proxy(engine_t& engine) 
    : engine_(&engine) 
  {}

  void set_id() {
    current_thread_id_ = engine_->register_thread_id();
  }
};


/**
 * Thread encapsulates an instance of an real thread
 *
 */
template <class StackTraits>
class thread {
  using routine_t = routine<StackTraits>;
  using routine_ptr_t = std::unique_ptr<routine_t>;
  using engine_t = engine<StackTraits>;
  using engine_proxy_t = engine_proxy<StackTraits>;
  using command_t = thread_command<StackTraits>;
  using engine_queue_t = queues::weakrb<command_t, 100>;

  engine_proxy_t engine_proxy_;
  std::vector<routine_ptr_t> scheduled_routines_;
  thread_status status_{thread_status::idle};
  uv_loop_t uv_loop_;

  engine_queue_t engine_queue_;
  uv_async_t engine_async_handle_;
  uv_async_t self_handle_;

  // C style callback shim
  static void handle_engine_async_send(uv_async_t* handle) {
    reinterpret_cast<thread*>(handle->data)->handle_engine_async_send();
  };

  // Member function called by its matching C-Style callbacl
  void handle_engine_async_send() {
    command_t received_command;
    while (engine_queue_.pop(received_command)) {
      switch (received_command.type) {
        case thread_command_type::add_routine:
          scheduled_routines_.emplace_back(received_command.data.template get<routine_ptr_t>().release());
          break;
        case thread_command_type::finish:
          status_ = thread_status::finished;
          break;
      }
    }
    execute_scheduled_routines();
  }

  static void handle_self_send(uv_async_t* handle) {
    reinterpret_cast<thread*>(handle->data)->handle_self_send();
  };

  void handle_self_send() {
    execute_scheduled_routines();
  }

  void close_handles() {
    uv_close(reinterpret_cast<uv_handle_t*>(&engine_async_handle_),nullptr);
    uv_close(reinterpret_cast<uv_handle_t*>(&self_handle_),nullptr);
  }

 public:
  thread(thread const&) = delete;
  thread(thread&&) = default;
  thread& operator=(thread const&) = delete;
  thread& operator=(thread&&) = default;

  // callaed by engine
  bool push_command(command_t& command) {
    return engine_queue_.push(command);
    //return true;
  };

  // called by engine
  void execute_commands() {
    uv_async_send(&engine_async_handle_);
  }

  thread(engine_t& engine) : engine_proxy_(engine) {
    uv_loop_init(&uv_loop_);
    uv_async_init(&uv_loop_, &engine_async_handle_, thread::handle_engine_async_send);
    engine_async_handle_.data = this; 
    uv_async_init(&uv_loop_, &self_handle_, thread::handle_engine_async_send);
    self_handle_.data = this;
  }

  ~thread() {
    auto rc = uv_loop_close(&uv_loop_);
    //assert(0 == rc);
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
    if (0 == scheduled_routines_.size() && thread_status::finished == status_) {
      close_handles();
    }
    else {
      // Re schedule a loop
      uv_async_send(&self_handle_);
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
    uv_run(&uv_loop_, UV_RUN_DEFAULT);
  }

  inline void operator()() {
    loop();
  };
};

}  // namespace context
}  // namespace boson

#endif  // BOSON_THREAD_H_ 
