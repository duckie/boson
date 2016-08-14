#ifndef BOSON_ROUTINE_H_
#define BOSON_ROUTINE_H_
#pragma once

#include "stack.h"
#include "fcontext.h"

namespace boson {

enum class routine_status {
  is_new,   // Routine has been created bbut never started
  started,  // Routine started
  finished  // Routine finished execution
};

template <class StackTraits = stack::default_stack_traits>
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
template <class StackTraits>
void resume_routine(context::transfer_t transfered_context) {
  using routine_t = routine<StackTraits>;
  routine_context rcontext{transfered_context};
  auto current_routine = reinterpret_cast<routine_t*>(transfered_context.data);
  (*current_routine->func_)(rcontext);
  current_routine->status_ = routine_status::finished;
  context::jump_fcontext(transfered_context.fctx, nullptr);
}

struct function_holder {
  virtual void operator() (routine_context&) = 0;
};

template <class Function> class function_holder_impl : public function_holder {
  Function func_;

  public:
  function_holder_impl(Function&& func) : func_{std::move(func)} {}
  void operator() (routine_context& context) override {
    func_(context);
  }
};
}  // nemesapce detail


/**
 * routine represents a signel unit of execution
 *
 */
template <class StackTraits>
class routine {
  friend void detail::resume_routine<StackTraits>(context::transfer_t);
  std::unique_ptr<detail::function_holder> func_;
  stack::stack_context stack_;
  routine_status status_;
  context::transfer_t context_;

 public:
  template <class Function> routine(Function&& func)
      : func_{new detail::function_holder_impl<Function>(std::forward<Function>(func))},
        stack_{stack::allocate<StackTraits>()},
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
        context_.fctx = context::make_fcontext(stack_.sp, stack_.size, detail::resume_routine<StackTraits>);
        status_ = routine_status::started;
        context_ = context::jump_fcontext(context_.fctx, this);
        break;
      }
      case routine_status::started: {
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
}

#endif  // BOSON_ROUTINE_H_
