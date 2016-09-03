#ifndef BOSON_ROUTINE_H_
#define BOSON_ROUTINE_H_
#pragma once

#include "stack.h"
#include "fcontext.h"

namespace boson {

/**
 * Store the local thread context
 *
 * The usage of this breaks encapsulation sinces routines are not supposed
 * to know about what is runniung them. But it will avoid too much load/store
 * from a thread_local variable since it implies a look in a map
 */
thread_local context::transfer_t current_thread_context = { nullptr, nullptr };

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
  context::transfer_t& main_context = current_thread_context;
  main_context = transfered_context;
  //(*current_routine->func_)(rcontext);
  (*current_routine->func_)();
  current_routine->status_ = routine_status::finished;
  // It is paramount to use reference to the thread local variable here
  // since it can be updated during ping/ping context jumps during the routine
  // execution
  context::jump_fcontext(main_context.fctx, main_context.data);
}

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

void yield() {
  context::transfer_t& main_context = current_thread_context;
  //context::jump_fcontext(main_context.fctx,nullptr);
  main_context = context::jump_fcontext(main_context.fctx,nullptr);
}


}

#endif  // BOSON_ROUTINE_H_
