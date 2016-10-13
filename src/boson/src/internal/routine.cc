#include "internal/routine.h"
#include <cassert>
#include "exception.h"
#include "internal/thread.h"
#include "syscalls.h"

namespace boson {
namespace internal {

namespace detail {

// struct stack_header {
//};
//
void resume_routine(transfer_t transfered_context) {
  thread* this_thread = current_thread();
  this_thread->context() = transfered_context;
  routine* current_routine = this_thread->running_routine();
  current_routine->status_ = routine_status::running;
  (*current_routine->func_)();
  current_routine->status_ = routine_status::finished;
  // It is paramount to use reference to the thread local variable here
  // since it can be updated during ping/ping context jumps during the routine
  // execution
  // jump_fcontext(this_thread->context().fctx, this_thread->context().data);
  jump_fcontext(this_thread->context().fctx, nullptr);
}
}

// class routine;

routine::~routine() {
  deallocate(stack_);
}

void routine::resume(thread* managing_thread) {
  thread_ = managing_thread;
  switch (status_) {
    case routine_status::is_new: {
      // constexpr std::size_t func_alignment = 64; // alignof( record_t);
      // constexpr std::size_t func_size = sizeof(detail::stack_header);
      //// reserve space on stack
      // void * sp = static_cast< char * >( stack_.sp) - func_size - func_alignment;
      //// align sp pointer
      // std::size_t space = func_size + func_alignment;
      // sp = std::align( func_alignment, func_size, sp, space);
      //// calculate remaining size
      // const std::size_t size = stack_.size - ( static_cast< char * >( stack_.sp) - static_cast<
      // char * >( sp) );
      //// create fast-context
      // context_.fctx = make_fcontext(sp, size, detail::resume_routine);
      //// placment new for control structure on context-stack
      // auto rec = ::new ( sp) detail::stack_header;
      context_.fctx = make_fcontext(stack_.sp, stack_.size, detail::resume_routine);
      context_ = jump_fcontext(context_.fctx, nullptr);
      break;
    }
    case routine_status::yielding: {
      context_ = jump_fcontext(context_.fctx, nullptr);
      break;
    }
    case routine_status::finished: {
      // Not supposed to happen
      assert(false);
      break;
    }
  }

  // Manage the queue requests
  while (context_.data && status_ == routine_status::running) {
    in_context_function* func = static_cast<in_context_function*>(context_.data);
    (*func)(thread_);
    context_ = jump_fcontext(context_.fctx, nullptr);
  }
}

}  // namespace internal

}  // namespace boson
