#ifndef BOSON_THREAD_H_
#define BOSON_THREAD_H_
#pragma once

#include <cassert>
#include <json_backbone.hpp>
#include <list>
#include <map>
#include <memory>
#include <thread>
#include <vector>
#include <chrono>
#include "event_loop.h"
#include "queues/weakrb.h"
#include "routine.h"

namespace json_backbone {
template <> struct is_small_type<std::unique_ptr<boson::routine>> {
  constexpr static bool const value = true;
};
}

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

enum class thread_command_type { add_routine, finish };


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
  engine_proxy(engine&);
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
  std::list<routine_ptr_t> scheduled_routines_;
  thread_status status_{thread_status::idle};
  event_loop loop_;

  engine_queue_t engine_queue_;
  int engine_event_id_;
  int self_event_id_;
  
  /**
   * This map stores the timers
   *
   * The idea here is to avoid additional fd creation just for timers, so we can create
   * a whole lot of them without consuming the fd limit per process
   */
  std::map<routine_time_point, std::list<routine_ptr_t>> timed_routines_;

  /**
   * React to a request from tje main scheduler
   */
  void handle_engine_event();

  /**
   * Close event handlers to free the event loop
   */
  void unregister_all_events();

 public:
  thread(engine& parent_engine);
  thread(thread const&) = delete;
  thread(thread&&) = default;
  thread& operator=(thread const&) = delete;
  thread& operator=(thread&&) = default;
  ~thread() = default;

  // Event handler interface
  void event(int event_id, void* data) override;
  void read(int fd, void* data) override;
  void write(int fd, void* data) override;

  // callaed by engine
  bool push_command(thread_command& command);

  // called by engine
  void execute_commands();

  void execute_scheduled_routines();
  void loop();
};

}  // namespace context
}  // namespace boson

#endif  // BOSON_THREAD_H_
