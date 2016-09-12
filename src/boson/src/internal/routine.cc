#include "internal/routine.h"
#include <cassert>
#include "exception.h"
#include "syscalls.h"

namespace boson {
namespace internal {

namespace detail {
void resume_routine(transfer_t transfered_context) {
  auto current_routine = static_cast<routine*>(transfered_context.data);
  transfer_t& main_context = current_thread_context;
  main_context = transfered_context;
  current_routine->status_ = routine_status::running;
  (*current_routine->func_)();
  current_routine->status_ = routine_status::finished;
  // It is paramount to use reference to the thread local variable here
  // since it can be updated during ping/ping context jumps during the routine
  // execution
  jump_fcontext(main_context.fctx, main_context.data);
}
}

// class routine;

routine::~routine() {
  deallocate(stack_);
}

void routine::resume() {
  switch (status_) {
    case routine_status::is_new: {
      context_.fctx = make_fcontext(stack_.sp, stack_.size, detail::resume_routine);
      context_ = jump_fcontext(context_.fctx, this);
      break;
    }
    case routine_status::yielding: {
      context_ = jump_fcontext(context_.fctx, this);
      break;
    }
    case routine_status::finished: {
      // Not supposed to happen
      assert(false);
      break;
    }
  }
}

}  // namespace internal

// Free functions

using namespace internal;

void yield() {
  transfer_t& main_context = current_thread_context;
  routine* current_routine = static_cast<routine*>(main_context.data);
  current_routine->status_ = routine_status::yielding;
  main_context = jump_fcontext(main_context.fctx, nullptr);
  current_routine->previous_status_ = routine_status::yielding;
  current_routine->status_ = routine_status::running;
}

void sleep(std::chrono::milliseconds duration) {
  // Compute the time in ms
  using namespace std::chrono;
  transfer_t& main_context = current_thread_context;
  routine* current_routine = static_cast<routine*>(main_context.data);
  current_routine->waiting_data_ = time_point_cast<milliseconds>(high_resolution_clock::now() + duration);
  current_routine->status_ = routine_status::wait_timer;
  main_context = jump_fcontext(main_context.fctx, nullptr);
  current_routine->previous_status_ = routine_status::wait_timer;
  current_routine->status_ = routine_status::running;
}

ssize_t read(int fd, void *buf, size_t count) {
  transfer_t& main_context = current_thread_context;
  routine* current_routine = static_cast<routine*>(main_context.data);
  routine_io_event new_event{fd,-1,false};
  if (current_routine->previous_status_is_io_block()) {
    auto previous_event = current_routine->waiting_data_.raw<routine_io_event>();
    new_event.event_id = previous_event.event_id;
    if (previous_event.fd == fd) {
      new_event.is_same_as_previous_event = true;
    }
  }
  current_routine->waiting_data_ = new_event;
  current_routine->status_ = routine_status::wait_sys_read;
  main_context = jump_fcontext(main_context.fctx, nullptr);
  current_routine->previous_status_ = routine_status::wait_sys_read;
  current_routine->status_ = routine_status::running;
  return ::read(fd,buf,count);
}

ssize_t write(int fd, const void *buf, size_t count) {
  transfer_t& main_context = current_thread_context;
  routine* current_routine = static_cast<routine*>(main_context.data);
  routine_io_event new_event{fd,-1,false};
  if (current_routine->previous_status_is_io_block()) {
    auto previous_event = current_routine->waiting_data_.raw<routine_io_event>();
    new_event.event_id = previous_event.event_id;
    if (previous_event.fd == fd) {
      new_event.is_same_as_previous_event = true;
    }
  }
  current_routine->waiting_data_ = new_event;
  current_routine->status_ = routine_status::wait_sys_write;
  main_context = jump_fcontext(main_context.fctx, nullptr);
  current_routine->previous_status_ = routine_status::wait_sys_write;
  current_routine->status_ = routine_status::running;
  return ::write(fd,buf,count);
}

}  // namespace boson
