#ifndef BOSON_THREAD_H_
#define BOSON_THREAD_H_
#pragma once

#include <list>
#include <memory>
#include <thread>
#include "routine.h"

namespace boson {

template <class StackTraits> class engine;

namespace context {

enum class thread_status {
  idle,     // Thread waits to be unlocked
  busy,     // Thread executes a routine
  finished  // Thread no longer executes a routine and is not required to wait
};

enum class thread_command {
  loop,   // Tells the thread to wait again for routines when finished
  finish  // Tells the thread to terminate when finished
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

  engine_t& engine_;
  typename engine_t::
  thread_id_t current_thread_id_ {};

 public:
  engine_proxy(engine_t& engine) : engine_(engine) {}

  void set_id() {
    current_thread_id_ = engine_.register_thread_id();
  }

  thread_command wait_engine_command() {
    return engine_.wait_engine_command(current_thread_id_);
  }
};


/**
 * Thread encapsulates an instance of an real thread
 *
 */
template <class StackTraits>
class thread {
  using routine_t = routine<StackTraits>;
  using engine_t = engine<StackTraits>;
  using engine_proxy_t = engine_proxy<StackTraits>;

  engine_proxy_t engine_proxy_;
  std::list<std::unique_ptr<routine_t>> scheduled_routines_;
  thread_status status_{thread_status::idle};

 public:
  thread(thread const&) = delete;
  thread(thread&&) = default;
  thread& operator=(thread const&) = delete;
  thread& operator=(thread&&) = default;

  thread(engine_t& engine) : engine_proxy_(engine) {
  }

  ~thread() {
  };

  template <class Function>
  void start(Function&& function) {
    scheduled_routines_.emplace_back(new routine<StackTraits>(std::forward<Function>(function)));
  };

  void execute_scheduled_routines() {
    while (!scheduled_routines_.empty()) {
      // For now; we schedule them in order
      std::unique_ptr<routine_t> current_routine{scheduled_routines_.front().release()};
      scheduled_routines_.pop_front();
      current_routine->resume();
      if (routine_status::finished != current_routine->status()) {
        // If not finished, then we reschedule it
        scheduled_routines_.emplace_back(std::move(current_routine));
      }
    }
  }

  void loop() {
    engine_proxy_.set_id();  // Tells the engine which thread id we got 
    do {
      switch (status_) {
        case thread_status::idle:
          {
          thread_command command = engine_proxy_.wait_engine_command();
          status_ = thread_status::busy;
          if (thread_command::finish == command) {
            status_ = thread_status::finished;
          }
          }
          break;
        case thread_status::busy:
          execute_scheduled_routines();
          status_ = thread_status::idle;
          break;
        case thread_status::finished:
          execute_scheduled_routines();
          break;
      }
    } while(thread_status::finished != status_);
  }

  inline void operator()() {
    loop();
  };
};

}  // namespace context
}  // namespace boson

#endif  // BOSON_THREAD_H_ 
