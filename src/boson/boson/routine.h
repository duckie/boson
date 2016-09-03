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

/**
 * Data published to parent threads about waiting reasons
 *
 * When a routine yields to wait on a timer, a fd or a channel, it needs
 * tell its running thread what it is about.
 */
using routine_waiting_data = json_backbone::variant<std::nullptr_t,size_t>;

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
};

void yield();

/**
 * Suspend the routine for the givent duration
 *
 * Thougn this is a genric duration, timers have the system clock
 * granularity and cannot be more accurate than 1 millisecond
 */
template <class Duration> void sleep(Duration&& duration) {
  // Compute the time in ms
  using namespace std::chrono;
  //size_t duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
  context::transfer_t& main_context = current_thread_context;
  routine* current_routine = static_cast<routine*>(main_context.data);
  current_routine->waiting_data_ = duration_cast<milliseconds>(high_resolution_clock::now() + duration).count();
  current_routine->status_ = routine_status::wait_timer;
  main_context = context::jump_fcontext(main_context.fctx, nullptr);
}

// Inline implementations

routine_status routine::status() {
  return status_;
}

routine_waiting_data const& routine::waiting_data() const {
  return waiting_data_;
  }
}

#endif  // BOSON_ROUTINE_H_
