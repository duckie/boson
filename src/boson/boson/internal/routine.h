#ifndef BOSON_ROUTINE_H_
#define BOSON_ROUTINE_H_
#pragma once

#include <chrono>
#include <json_backbone.hpp>
#include <memory>
#include "boson/syscalls.h"
#include "fcontext.h"
#include "stack.h"

namespace boson {
using routine_id = std::size_t;
namespace internal {

class thread;

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
  wait_timer,      // Routine waits for a timer to expire
  wait_sys_read,   // Routine waits for a FD to be ready for read
  wait_sys_write,  // Routine waits for a FD to be readu for write
  wait_sema_wait,  // Routine waits to get a boson::semaphore
  //wait_channek_write,
  //wait_channel_read,
  finished  // Routine finished execution
};

using routine_time_point =
    std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::milliseconds>;

struct routine_io_event {
  int fd;                          // The current FD used
  int event_id;                    // The id used for the event loop
  bool is_same_as_previous_event;  // Used to limit system calls in loops
};
}
}

namespace json_backbone {
template <>
struct is_small_type<boson::internal::routine_time_point> {
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
    json_backbone::variant<std::nullptr_t, int, size_t, routine_io_event, routine_time_point>;

class routine;

namespace detail {
void resume_routine(transfer_t transfered_context);

struct function_holder {
  virtual ~function_holder() = default;
  virtual void operator()() = 0;
};

template <class Function>
class function_holder_impl : public function_holder {
  Function func_;

 public:
  function_holder_impl(Function&& func) : func_{std::move(func)} {
  }
  void operator()() override {
    func_();
  }
};
}  // nemespace detail

/**
 * routine represents a single unit of execution
 *
 */
class routine {
  friend void detail::resume_routine(transfer_t);
  friend void boson::yield();
  friend void boson::sleep(std::chrono::milliseconds);
  friend ssize_t boson::read(int fd, void* buf, size_t count);
  friend ssize_t boson::write(int fd, const void* buf, size_t count);
  template <class ContentType>
  friend class channel;
  friend class thread;
  friend class boson::semaphore;

  std::unique_ptr<detail::function_holder> func_;
  stack_context stack_ = allocate<default_stack_traits>();
  routine_status previous_status_ = routine_status::is_new;
  routine_status status_ = routine_status::is_new;
  routine_waiting_data waiting_data_;
  transfer_t context_;
  thread* thread_;
  routine_id id_;

 public:
  template <class Function>
  routine(routine_id id, Function&& func)
      : func_{new detail::function_holder_impl<Function>(std::forward<Function>(func))}, id_{id} {
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
   * Tells the routine it can be executed
   *
   * When the wait is over, the routine must be informed it can
   * execute by putting its status to "yielding"
   */
  inline void expected_event_happened();
};

static thread_local thread* this_routine = nullptr;

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

}  // namespace internal
}  // namespace boson

#endif  // BOSON_ROUTINE_H_
