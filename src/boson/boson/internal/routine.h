#ifndef BOSON_ROUTINE_H_
#define BOSON_ROUTINE_H_
#pragma once

#include <chrono>
#include <json_backbone.hpp>
#include <memory>
#include "boson/std/experimental/apply.h"
#include "boson/syscalls.h"
#include "boson/utility.h"
#include "boson/memory/local_ptr.h"
#include "fcontext.h"
#include "stack.h"
#include <vector>
#include "../event_loop.h"

namespace boson {

namespace queues {
class base_wfqueue;
}

using routine_id = std::size_t;
class semaphore;

namespace internal {
class routine;
class thread;
class timed_routines_set;
}

using routine_ptr_t = std::unique_ptr<internal::routine>;
using routine_local_ptr_t = memory::local_ptr<std::unique_ptr<internal::routine>>;

namespace internal {
/**
 * Store the local thread context
 *
 * The usage of this breaks encapsulation sinces routines are not supposed
 * to know about what is runniung them. But it will avoid too much load/store
 * from a thread_local variable since it implies a look in a map
 */
// static thread_local transfer_t current_thread_context = {nullptr, nullptr};

enum class routine_status {
  is_new,          // Routine has been created but never started
  running,         // Routine is currently running
  yielding,        // Routine yielded and waits to be resumed
  timed_out,       // Status when a routine waited for an event and timed out
  wait_events,     // Routine awaits some events
  wait_sys_read,   // Routine waits for a FD to be ready for read
  wait_sys_write,  // Routine waits for a FD to be readu for write
  wait_sema_wait,  // Routine waits to get a boson::semaphore
  finished         // Routine finished execution
};

using routine_time_point =
    std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::milliseconds>;

enum class event_type {
  none,
  timer,
  io_read,
  io_write,
  sema_wait
};

struct routine_timer_event_data {
  routine_time_point date;
  timed_routines_set* neighbor_timers;
};

struct routine_sema_event_data {
  semaphore* sema;
};

struct routine_io_event {
  int fd;                          // The current FD used
  int event_id;                    // The id used for the event loop
  bool is_same_as_previous_event;  // Used to limit system calls in loops
  bool panic;                      // True if event loop answered in panic to this event
};




}
}

namespace json_backbone {
template <>
struct is_small_type<boson::internal::routine_time_point> {
  constexpr static bool const value = true;
};
template <>
struct is_small_type<boson::internal::routine_timer_event_data> {
  constexpr static bool const value = true;
};
template <>
struct is_small_type<boson::internal::routine_io_event> {
  constexpr static bool const value = true;
};
}

namespace boson {

class semaphore;

namespace internal {



/**
 * Data published to parent threads about waiting reasons
 *
 * When a routine yields to wait on a timer, a fd or a channel, it needs
 * tell its running thread what it is about.
 */
using routine_waiting_data =
    json_backbone::variant<std::nullptr_t, int, size_t, routine_io_event, routine_timer_event_data, routine_sema_event_data>;

class routine;

namespace detail {
void resume_routine(transfer_t transfered_context);

struct function_holder {
  virtual ~function_holder() = default;
  virtual void operator()() = 0;
};

template <class Function, class... Args>
class function_holder_impl : public function_holder {
  Function func_;
  std::tuple<Args...> args_;

 public:
  function_holder_impl(Function func, Args... args)
      : func_{std::move(func)}, args_{std::forward<Args>(args)...} {
  }
  void operator()() override {
    return experimental::apply(func_, std::move(args_));
  }
};

template <class Function, class... Args>
decltype(auto) make_unique_function_holder(Function&& func, Args&&... args) {
  return std::unique_ptr<function_holder>(new function_holder_impl<Function, Args...>(
      std::forward<Function>(func), std::forward<Args>(args)...));
}
}  // namespace detail

struct in_context_function {
  virtual ~in_context_function() = default;
  virtual void operator()(thread* this_thread) = 0;
};

struct queue_request {
  queues::base_wfqueue* queue;
  void* data;
};

/**
 * routine represents a single unit of execution
 *
 */
class routine {
  friend void detail::resume_routine(transfer_t);
  friend void boson::yield();
  friend void boson::sleep(std::chrono::milliseconds);
  friend int boson::wait_readiness(fd_t,bool);
  template <class ContentType>
  friend class channel;
  friend class thread;
  friend class boson::semaphore;

  struct waited_event {
    event_type type;
    routine_waiting_data data;
    //waited_event() = default;
    //waited_event(waited_event const&) = default;
    //waited_event(waited_event&&) = default;
    //waited_event& operator=(waited_event const&) = default;
    //waited_event& operator=(waited_event&&) = default;
  };


  std::unique_ptr<detail::function_holder> func_;
  stack_context stack_ = allocate<default_stack_traits>();
  routine_status previous_status_ = routine_status::is_new;
  routine_status status_ = routine_status::is_new;
  routine_waiting_data waiting_data_;
  transfer_t context_;
  thread* thread_;
  routine_id id_;
  std::vector<waited_event> previous_events_;
  std::vector<waited_event> events_;
  routine_local_ptr_t current_ptr_;

 public:
  template <class Function, class... Args>
  routine(routine_id id, Function&& func, Args&&... args)
      : func_{detail::make_unique_function_holder(std::forward<Function>(func),
                                                  std::forward<Args>(args)...)},
        id_{id} {
  }

  routine(routine const&) = delete;
  routine(routine&&) = default;
  routine& operator=(routine const&) = delete;
  routine& operator=(routine&&) = default;

  ~routine();

  /**
   * Returns the current status
   */
  inline routine_id id() const;
  inline routine_status previous_status() const;
  inline bool previous_status_is_io_block() const;
  inline routine_status status() const;
  inline routine_waiting_data& waiting_data();
  inline routine_waiting_data const& waiting_data() const;


  void start_event_round();
  void add_semaphore_wait(semaphore* sema);
  void add_timer(routine_time_point date);
  void commit_event_round();

  /**
   * Returns true if event is enough to trigger routine for scheduling
   */
  bool event_happened(std::size_t index);

  /**
   * Starts or resume the routine
   *
   * If the routinne si not yet started, it is satrted, otherwise, it
   * is resumed. This is a key function since this is where the
   * context execution will hang when the routine executes
   *
   */
  void resume(thread* managing_thread);
  // void queue_write(queues::base_wfqueue& queue, void* data);
  // void* queue_read(queues::base_wfqueue& queue);

  /**
   * Tells the routine it can be executed
   *
   * When the wait is over, the routine must be informed it can
   * execute by putting its status to "yielding"
   */
  inline void expected_event_happened();

  inline void timed_out();
  // void execute_in
};

// Inline implementations
routine_id routine::id() const {
  return id_;
}

routine_status routine::previous_status() const {
  return previous_status_;
}

bool routine::previous_status_is_io_block() const {
  return previous_status_ == routine_status::wait_sys_read ||
         previous_status_ == routine_status::wait_sys_write;
}

routine_status routine::status() const {
  return status_;
}

routine_waiting_data& routine::waiting_data() {
  return waiting_data_;
}

routine_waiting_data const& routine::waiting_data() const {
  return waiting_data_;
}

void routine::expected_event_happened() {
  status_ = routine_status::yielding;
}

void routine::timed_out() {
  if (status_ == routine_status::wait_events)
    status_ = routine_status::yielding;
  else
    status_ = routine_status::timed_out;
}

}  // namespace internal
}  // namespace boson

#endif  // BOSON_ROUTINE_H_
