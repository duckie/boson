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
  // since it can be updated during ping/pong context jumps during the routine
  // execution
  // jump_fcontext(this_thread->context().fctx, this_thread->context().data);
  jump_fcontext(this_thread->context().fctx, nullptr);
}
}

// class routine;

routine::~routine() {
  deallocate(stack_);
}

void routine::start_event_round() {
  // Clean previous events
  previous_events_.clear();
  std::swap(previous_events_, events_);
}

void routine::add_timer(routine_time_point date) {
  events_.emplace_back(waited_event{event_type::timer, routine_timer_event_data{std::move(date),nullptr}});
}

void routine::commit_event_round() {
  routine_local_ptr_t routine_slot(std::unique_ptr<routine>(this));
  for (auto& event : events_) {
    switch (event.type) {
      case event_type::none:
        break;
      case event_type::timer:
        event.data.get<routine_timer_event_data>().neighbor_timers =
            &thread_->register_timer(event.data.get<routine_timer_event_data>().date, routine_slot);
        //debug::log("Registered a timer");
        break;
      case event_type::io_read:
        break;
      case event_type::io_write:
        break;
      case event_type::sema_wait:
        break;
    }
  }
}

void routine::resume(thread* managing_thread) {
  thread_ = managing_thread;
  switch (status_) {
    case routine_status::is_new: {
      context_.fctx = make_fcontext(stack_.sp, stack_.size, detail::resume_routine);
      context_ = jump_fcontext(context_.fctx, nullptr);
      break;
    }
    case routine_status::yielding: {
      context_ = jump_fcontext(context_.fctx, nullptr);
      break;
    }
    case routine_status::timed_out: {
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
