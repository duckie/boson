#ifndef BOSON_THREAD_H_
#define BOSON_THREAD_H_
#pragma once

#include <cassert>
#include <chrono>
#include <json_backbone.hpp>
#include <list>
#include <map>
#include <memory>
#include <thread>
#include <vector>
#include "boson/event_loop.h"
#include "boson/queues/mpmc.h"
#include "boson/queues/weakrb.h"
#include "routine.h"

namespace json_backbone {
template <>
struct is_small_type<std::unique_ptr<boson::internal::routine>> {
  constexpr static bool const value = true;
};
}

namespace boson {

class engine;
using thread_id = std::size_t;

namespace internal {

enum class thread_status {
  idle,       // Thread waits to be unlocked
  busy,       // Thread executes a routine
  finishing,  // Thread no longer executes a routine and is not required to wait
  finished    // Thread no longer executes a routine and is not required to wait
};

enum class thread_command_type { add_routine, schedule_waiting_routine, finish };

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
  inline thread_id get_id() const { return current_thread_id_; }
};

/**
 * Thread encapsulates an instance of an real thread
 *
 */
class thread : public event_handler {
  friend void detail::resume_routine(transfer_t);
  friend void boson::yield();
  friend void boson::sleep(std::chrono::milliseconds);
  friend ssize_t boson::read(int fd, void* buf, size_t count);
  friend ssize_t boson::write(int fd, const void* buf, size_t count);
  template <class ContentType>
  friend class channel;

  friend class boson::semaphore;
  using routine_ptr_t = std::unique_ptr<routine>;
  using engine_queue_t = queues::bounded_mpmc<thread_command>;

  engine_proxy engine_proxy_;
  std::list<routine_ptr_t> scheduled_routines_;
  thread_status status_{thread_status::idle};

  /**
   * Execution context used to jump betweent thread and its routines
   *
   * Useful to get it from the TLS
   */
  transfer_t context_;

  /**
   * Holds a pointer to the currently runninf routine
   *
   * Useful to get it from the TLS
   */
  routine* running_routine_;

  /**
   * Event loop managing interruptions
   */
  event_loop loop_;

  engine_queue_t engine_queue_{1000};
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
   * Stores the number of suspended routines
   *
   * This number contains the number of suspended routines stored
   * in the event loop. The event loop holds the routines waiting for
   * events of its FDs.
   *
   * This also counts routines waiting in a semaphore waiters list.
   */
  size_t suspended_routines_{0};

  /**
   * React to a request from the main scheduler
   */
  void handle_engine_event();

  /**
   * Close event handlers to free the event loop
   */
  void unregister_all_events();

  inline transfer_t& context();
  inline routine* running_routine();

 public:
  thread(engine& parent_engine);
  thread(thread const&) = delete;
  thread(thread&&) = default;
  thread& operator=(thread const&) = delete;
  thread& operator=(thread&&) = default;
  ~thread() = default;

  inline thread_id id() const;

  // Event handler interface
  void event(int event_id, void* data) override;
  void read(int fd, void* data) override;
  void write(int fd, void* data) override;

  // called by engine
  void push_command(thread_command&& command);

  // called by engine
  // void execute_commands();

  void execute_scheduled_routines();

  /**
   * Executes the boson::thread
   *
   * loop() is executed in a std::thread dedicated to this object
   */
  void loop();
};

/**
 * Stores current thread in the TLS
 *
 * This is not convenient, from a programmatic standpoint, to use
 * the notion of current_thread, as a current_routine would be better
 *
 * But it avoids TLS writes at each context switch
 */
thread*& current_thread();

transfer_t& thread::context() {
  return context_;
}

routine* thread::running_routine() {
  return running_routine_;
}


thread_id thread::id() const {
  return engine_proxy_.get_id();
}

}  // namespace internal
}  // namespace boson

#endif  // BOSON_THREAD_H_
