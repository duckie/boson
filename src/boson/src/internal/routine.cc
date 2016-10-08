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
  jump_fcontext(this_thread->context().fctx, this_thread->context().data);
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
  while (status_ == routine_status::request_queue_push ||
         status_ == routine_status::request_queue_pop) {
    queue_request* request = static_cast<queue_request*>(context_.data);
    if (status_ == routine_status::request_queue_push) {
      request->queue->push(thread_->id(), request->data);
    } else {
      request->data = request->queue->pop(thread_->id());
    }
    context_ = jump_fcontext(context_.fctx, nullptr);
  }
}

void routine::queue_push(queues::base_wfqueue& queue, void* data) {
  queue_request* request = new queue_request{&queue, data};
  status_ = routine_status::request_queue_push;
  thread_->context() = jump_fcontext(thread_->context().fctx, request);
  delete request;
  status_ = routine_status::running;
}

void* routine::queue_pop(queues::base_wfqueue& queue) {
  queue_request* request = new queue_request{&queue, nullptr};
  status_ = routine_status::request_queue_pop;
  thread_->context() = jump_fcontext(thread_->context().fctx, request);
  status_ = routine_status::running;
  void* data = request->data;
  delete request;
  return data;
}

}  // namespace internal

// Free functions

using namespace internal;

void yield() {
  thread* this_thread = current_thread();
  routine* current_routine = this_thread->running_routine();
  current_routine->status_ = routine_status::yielding;
  this_thread->context() = jump_fcontext(this_thread->context().fctx, nullptr);
  current_routine->previous_status_ = routine_status::yielding;
  current_routine->status_ = routine_status::running;
}

void sleep(std::chrono::milliseconds duration) {
  // Compute the time in ms
  using namespace std::chrono;
  thread* this_thread = current_thread();
  routine* current_routine = this_thread->running_routine();
  current_routine->waiting_data_ =
      time_point_cast<milliseconds>(high_resolution_clock::now() + duration);
  current_routine->status_ = routine_status::wait_timer;
  this_thread->context() = jump_fcontext(this_thread->context().fctx, nullptr);
  current_routine->previous_status_ = routine_status::wait_timer;
  current_routine->status_ = routine_status::running;
}

ssize_t read(int fd, void* buf, size_t count) {
  thread* this_thread = current_thread();
  routine* current_routine = this_thread->running_routine();
  routine_io_event new_event{fd, -1, false};
  if (current_routine->previous_status_is_io_block()) {
    auto previous_event = current_routine->waiting_data_.raw<routine_io_event>();
    new_event.event_id = previous_event.event_id;
    if (previous_event.fd == fd) {
      new_event.is_same_as_previous_event = true;
    }
  }
  current_routine->waiting_data_ = new_event;
  current_routine->status_ = routine_status::wait_sys_read;
  this_thread->context() = jump_fcontext(this_thread->context().fctx, nullptr);
  current_routine->previous_status_ = routine_status::wait_sys_read;
  current_routine->status_ = routine_status::running;
  return ::read(fd, buf, count);
}

ssize_t write(int fd, const void* buf, size_t count) {
  thread* this_thread = current_thread();
  routine* current_routine = this_thread->running_routine();
  routine_io_event new_event{fd, -1, false};
  if (current_routine->previous_status_is_io_block()) {
    auto previous_event = current_routine->waiting_data_.raw<routine_io_event>();
    new_event.event_id = previous_event.event_id;
    if (previous_event.fd == fd) {
      new_event.is_same_as_previous_event = true;
    }
  }
  current_routine->waiting_data_ = new_event;
  current_routine->status_ = routine_status::wait_sys_write;
  this_thread->context() = jump_fcontext(this_thread->context().fctx, nullptr);
  current_routine->previous_status_ = routine_status::wait_sys_write;
  current_routine->status_ = routine_status::running;
  return ::write(fd, buf, count);
}

}  // namespace boson
