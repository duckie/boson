#ifndef BOSON_ROUTINE_H_
#define BOSON_ROUTINE_H_
#pragma once

#include "stack.h"
#include "fcontext.h"
#include <memory>

namespace boson {

enum class routine_status {
  is_new,              // Routine has been created bbut never started
  running,             // Routine started
  yielding,            // Routine yielded and waiths to be resumed
  wait_sys_read,       // Routine waits for a FD to be ready for read
  wait_sys_write,      // Routine waits for a FD to be readu for write
  wait_channel_read,   // Routines waits for a sync or empty channel to have an element
  wait_channel_write,  // Routine waiths for a full channel to have space or a sync channel to have a reader
  finished             // Routine finished execution
};

class routine;

class routine_context {
  context::transfer_t context_;
 public:
  routine_context(context::transfer_t context) : context_{context} {
  }

  inline void yield() {
    context_ = context::jump_fcontext(context_.fctx,context_.data);  
  }
};


namespace detail {
void resume_routine(context::transfer_t transfered_context);

struct function_holder {
  //virtual void operator() (routine_context&) = 0;
  virtual void operator() () = 0;
};

template <class Function> class function_holder_impl : public function_holder {
  Function func_;

  public:
  function_holder_impl(Function&& func) : func_{std::move(func)} {}
  void operator() () override {
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
  std::unique_ptr<detail::function_holder> func_;
  stack::stack_context stack_;
  routine_status status_;
  context::transfer_t context_;

 public:
  template <class Function> routine(Function&& func)
      : func_{new detail::function_holder_impl<Function>(std::forward<Function>(func))},
        stack_{stack::allocate<stack::default_stack_traits>()},
        status_{routine_status::is_new} {
  }

  routine(routine const&) = delete;
  routine(routine&&) = default;
  routine& operator=(routine const&) = delete;
  routine& operator=(routine&&) = default;

  ~routine() {
    stack::deallocate(stack_);
  }

  /**
   * Returns the current status
   */
  routine_status status() {
    return status_;
  }

  /**
   * Starts or resume the routine
   *
   * If the routinne si not yet started, it is satrted, otherwise, it
   * is resumed. This is a key function since this is where the
   * context execution will hang when the routine executes
   *
   */
  void resume() {
    switch (status_) {
      case routine_status::is_new: {
        context_.fctx = context::make_fcontext(stack_.sp, stack_.size, detail::resume_routine);
        context_ = context::jump_fcontext(context_.fctx, this);
        break;
      }
      case routine_status::yielding: {
        context_ = context::jump_fcontext(context_.fctx, this);
        break;
      }
      case routine_status::finished: {
        // TODO: Maybe we should raise a critical error here
        break;
      }
    }
  }
};

void yield();

}

#endif  // BOSON_ROUTINE_H_
