#ifndef BOSON_ROUTINE_H_
#define BOSON_ROUTINE_H_
#pragma once

#include <chrono>
#include <memory>
#include <json_backbone.hpp>
#include "fcontext.h"
#include "stack.h"

namespace boson {

/**
 * Store the local thread context
 *
 * The usage of this breaks encapsulation sinces routines are not supposed
 * to know about what is runniung them. But it will avoid too much load/store
 * from a thread_local variable since it implies a look in a map
 */
static thread_local context::transfer_t current_thread_context = {nullptr, nullptr};

enum class routine_status {
  is_new,              // Routine has been created bbut never started
  running,             // Routine started
  yielding,            // Routine yielded and waiths to be resumed
  wait_timer,          // Routine waits  for a timer to expire
  wait_sys_read,       // Routine waits for a FD to be ready for read
  wait_sys_write,      // Routine waits for a FD to be readu for write
  wait_channel_read,   // Routines waits for a sync or empty channel to have an element
  wait_channel_write,  // Routine waits for a full channel to have space or a sync channel to have
                       // a reader
  finished             // Routine finished execution
};

using routine_time_point =
    std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::milliseconds>;

/**
 * Data published to parent threads about waiting reasons
 *
 * When a routine yields to wait on a timer, a fd or a channel, it needs
 * tell its running thread what it is about.
 */
using routine_waiting_data = json_backbone::variant<std::nullptr_t, size_t, routine_time_point>;

class routine;

namespace detail {
void resume_routine(context::transfer_t transfered_context);

struct function_holder {
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
}  // nemesapce detail

/**
 * routine represents a signel unit of execution
 *
 */
class routine {
  friend void detail::resume_routine(context::transfer_t);
  friend void yield();
  template <class Duration> friend void sleep(Duration&&);
  friend void sleep(std::chrono::milliseconds);
  std::unique_ptr<detail::function_holder> func_;
  stack::stack_context stack_;
  routine_status status_;
  routine_waiting_data waiting_data_;
  context::transfer_t context_;

 public:
  template <class Function>
  routine(Function&& func)
      : func_{new detail::function_holder_impl<Function>(std::forward<Function>(func))},
        stack_{stack::allocate<stack::default_stack_traits>()},
        status_{routine_status::is_new} {
  }

  routine(routine const&) = delete;
  routine(routine&&) = default;
  routine& operator=(routine const&) = delete;
  routine& operator=(routine&&) = default;

  ~routine();

  /**
   * Returns the current status
   */
  inline routine_status status();
  inline routine_waiting_data const& waiting_data() const;

  /**
   * Starts or resume the routine
   *
   * If the routinne si not yet started, it is satrted, otherwise, it
   * is resumed. This is a key function since this is where the
   * context execution will hang when the routine executes
   *
   */
  void resume();

  /**
   * Tells the routine it can be executed
   *
   * When the wait is over, the routine must be informed it can
   * execute by putting its status to "yielding"
   */
  inline void expected_event_happened();
};

void yield();

/**
 * Suspends the routine for the given duration
 */
void sleep(std::chrono::milliseconds duration);

// Inline implementations

routine_status routine::status() {
  return status_;
}

routine_waiting_data const& routine::waiting_data() const {
  return waiting_data_;
}

void routine::expected_event_happened() {
  status_ = routine_status::yielding;
}

}

#endif  // BOSON_ROUTINE_H_
