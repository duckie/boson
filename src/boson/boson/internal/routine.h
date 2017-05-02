#ifndef BOSON_ROUTINE_H_
#define BOSON_ROUTINE_H_
#pragma once

#include <chrono>
#include <memory>
#include <vector>
#include "boson/std/experimental/apply.h"
#include "boson/syscalls.h"
#include "boson/utility.h"
#include "boson/memory/local_ptr.h"
#include "fcontext.h"
#include "stack.h"
#include "../event_loop.h"
#include "../external/json_backbone.hpp"

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
  is_new,                // Routine has been created but never started
  running,               // Routine is currently running
  yielding,              // Routine yielded and waits to be resumed
  wait_events,           // Routine awaits some events
  sema_event_candidate,  // Special status to use the routine as a scheduled one
  finished               // Routine finished execution
};

using routine_time_point =
    std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::milliseconds>;

enum class event_type {
  none,
  timer,
  io_read,
  io_write,
  sema_wait,
  sema_closed
  //io_read_panic,
  //io_write_panic
};

enum class fd_status {
  unknown,  // Never used
  ready,    // Has data for a read, ready for write for a write
  consumed  // No more data for a read, blocked for a write
};

struct routine_timer_event_data {
  routine_time_point date;
  timed_routines_set* neighbor_timers;
};

struct routine_sema_event_data {
  semaphore* sema;
  size_t index;
  size_t slot_index;
};

struct routine_io_event {
  int fd;                          // The current FD used
  int event_id;                    // The id used for the event loop
  fd_status read_status;
  fd_status write_status;
  //bool is_same_as_previous_event;  // Used to limit system calls in loops
  //bool panic;                      // True if event loop answered in panic to this event
  
};

}
}

namespace json_backbone {
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
  using ArgsTuple = typename extract_tuple_arguments<Function, Args...>::type;
  Function func_;
  ArgsTuple args_;

 public:
  function_holder_impl(Function&& func, Args... args)
      : func_{std::move(func)}, args_{std::forward<Args>(args)...} {
  }

  function_holder_impl(Function const& func, Args... args)
      : func_{func}, args_{std::forward<Args>(args)...} {
  }

  void operator()() override {
    return experimental::apply(func_, std::move(args_));
  }
};

template <class Function, class... Args>
decltype(auto) make_unique_function_holder(Function&& func, Args&&... args) {
  return std::unique_ptr<function_holder>(new function_holder_impl<std::decay_t<Function>, Args...>(
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
  friend void boson::usleep(std::chrono::microseconds);
  template <bool,bool> friend int boson::wait_readiness(fd_t,int);
  template <class ContentType>
  friend class channel;
  friend class thread;
  friend class boson::semaphore;

  struct waited_event {
    event_type type;
    routine_waiting_data data;
  };

  std::unique_ptr<detail::function_holder> func_;
  stack_context stack_ = allocate<default_stack_traits>();
  routine_status previous_status_ = routine_status::is_new;
  routine_status status_ = routine_status::is_new;
  transfer_t context_;
  thread* thread_;
  routine_id id_;
  std::vector<waited_event> previous_events_;
  std::vector<waited_event> events_;
  routine_local_ptr_t current_ptr_;
  event_type happened_type_ = event_type::none;
  event_status happened_rc_ = 0;
  size_t happened_index_ = 0;

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
  inline routine_status status() const;
  inline routine_waiting_data& waiting_data();
  inline routine_waiting_data const& waiting_data() const;


  // Clean up previous events and prepare routine to new set
  void start_event_round();

  // Add a semaphore wait in the current set
  void add_semaphore_wait(semaphore* sema);

  // Add a timeout event to the set
  void add_timer(routine_time_point date);

  void add_read(int fd);

  void add_write(int fd);

  // Effectively commits the event set and suspends the routine
  size_t commit_event_round();

  void cancel_event_round();

  void set_as_semaphore_event_candidate(std::size_t index);

  bool event_is_a_fd_wait(std::size_t index, int fd);

  // Called by the thread to tell an event happened
  bool event_happened(std::size_t index, event_status status = 0);

  void close_fd(int fd);

  /**
   * Starts or resume the routine
   *
   * If the routinne si not yet started, it is satrted, otherwise, it
   * is resumed. This is a key function since this is where the
   * context execution will hang when the routine executes
   *
   */
  void resume(thread* managing_thread);

  /**
   * Returns the index in the list of events
   * which triggered the context switch
   */
  inline size_t happened_index() const;

  /**
   * Returns the type of events that triggered
   * the context switch. This is used for error management
   */
  inline event_type happened_type() const;

  /**
   * Get the offset in the stack of the given pointer
   */
  std::size_t get_stack_offset(void* pointer);
};

// Inline implementations
routine_id routine::id() const {
  return id_;
}

routine_status routine::previous_status() const {
  return previous_status_;
}

routine_status routine::status() const {
  return status_;
}

size_t routine::happened_index() const {
    return happened_index_;
}

event_type routine::happened_type() const {
    return happened_type_;
}


}  // namespace internal
}  // namespace boson

#endif  // BOSON_ROUTINE_H_
