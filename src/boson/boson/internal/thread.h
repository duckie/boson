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
#include "boson/logger.h"
#include "boson/queues/weakrb.h"
#include "boson/queues/wfqueue.h"
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

using thread_command_data = json_backbone::variant<std::nullptr_t, std::unique_ptr<routine>,
                                                   std::tuple<int, int, std::unique_ptr<routine>>>;

struct thread_command {
  thread_command_type type;
  thread_command_data data;
  inline thread_command(thread_command_type new_type, thread_command_data new_data)
      : type(new_type), data(std::move(new_data)) {
  }
};

/**
 * engine_proxy represents and engine view from the thread
 *
 * It encapsulates the semantics for the thread to identify
 * on the engine. currently, this semantics is an id.
 */
class engine_proxy final {
  // Use a pointer here to get free move ctor and operator
  engine* engine_;
  thread_id current_thread_id_;

 public:
  engine_proxy(engine&);
  ~engine_proxy();
  void set_id();
  routine_id get_new_routine_id();
  void notify_end();
  void notify_idle(size_t nb_suspended_routines);
  void start_routine(std::unique_ptr<routine> new_routine);
  void start_routine(thread_id target_thread, std::unique_ptr<routine> new_routine);
  inline thread_id get_id() const {
    return current_thread_id_;
  }
  inline engine const& get_engine() const {
    return *engine_;
  }
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
  friend class routine;

  friend class boson::semaphore;
  using routine_ptr_t = std::unique_ptr<routine>;
  using engine_queue_t = queues::base_wfqueue;

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

  engine_queue_t engine_queue_;
  std::atomic<std::size_t> nb_pending_commands_{0};
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

 public:
  thread(engine& parent_engine);
  thread(thread const&) = delete;
  thread(thread&&) = default;
  thread& operator=(thread const&) = delete;
  thread& operator=(thread&&) = default;
  ~thread() = default;

  inline thread_id id() const;
  inline engine const& get_engine() const;

  // Event handler interface
  void event(int event_id, void* data) override;
  void read(int fd, void* data) override;
  void write(int fd, void* data) override;

  // called by engine
  void push_command(thread_id from, std::unique_ptr<thread_command> command);

  // called by engine
  // void execute_commands();

  void execute_scheduled_routines();

  /**
   * Executes the boson::thread
   *
   * loop() is executed in a std::thread dedicated to this object
   */
  void loop();

  template <class Function, class ... Args>
  void start_routine(Function&& func, Args&& ... args) {
    engine_proxy_.start_routine(std::make_unique<routine>(engine_proxy_.get_new_routine_id(),
                                                          std::forward<Function>(func), std::forward<Args>(args)...));
  }

  inline routine* running_routine();
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

engine const& thread::get_engine() const {
  return engine_proxy_.get_engine();
}

}  // namespace internal

template <class Function, class ... Args>
void start(Function&& func, Args&& ... args) {
  internal::current_thread()->start_routine(std::forward<Function>(func), std::forward<Args>(args)...);
}

}  // namespace boson

#endif  // BOSON_THREAD_H_
