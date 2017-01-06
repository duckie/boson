#include "internal/routine.h"
#include <cassert>
#include "exception.h"
#include "internal/thread.h"
#include "syscalls.h"
#include "semaphore.h"

namespace boson {
namespace internal {

namespace detail {

void resume_routine(transfer_t transfered_context) {
  thread* this_thread = current_thread();
  this_thread->context() = transfered_context;
  routine* current_routine = this_thread->running_routine();
  current_routine->status_ = routine_status::running;
  (*current_routine->func_)();
  current_routine->status_ = routine_status::finished;
  jump_fcontext(this_thread->context().fctx, nullptr);
}
}

// class routine;

routine::~routine() {
  deallocate(stack_);
}

void routine::start_event_round() {
  // Clean previous events
  //previous_events_.clear();
  //std::swap(previous_events_, events_);
  events_.clear();
  // Create new event pointer
  current_ptr_ = routine_local_ptr_t(std::unique_ptr<routine>(this));
}

void routine::add_semaphore_wait(semaphore* sema) {
  events_.emplace_back(waited_event{event_type::sema_wait, routine_sema_event_data{sema,0,0}});
  auto slot_index = thread_->register_semaphore_wait(routine_slot{current_ptr_,events_.size()-1});
  events_.back().data.get<routine_sema_event_data>().index = sema->write(thread_, slot_index);
  events_.back().data.get<routine_sema_event_data>().slot_index = slot_index;
  int result = sema->counter_.fetch_add(1,std::memory_order_release);
  if (0 <= result) {
    sema->pop_a_waiter(thread_);
  }
}

void routine::add_timer(routine_time_point date) {
  events_.emplace_back(waited_event{event_type::timer, routine_timer_event_data{std::move(date),nullptr}});
  auto& event = events_.back();
  event.data.get<routine_timer_event_data>().neighbor_timers =
      &thread_->register_timer(event.data.get<routine_timer_event_data>().date, routine_slot{current_ptr_,events_.size()-1});
}

void routine::add_read(int fd) {
  events_.emplace_back(waited_event{event_type::io_read, fd});
  thread_->register_read(fd, routine_slot{current_ptr_, events_.size() -1});
}

void routine::add_write(int fd) {
  events_.emplace_back(waited_event{event_type::io_write, fd});
  thread_->register_write(fd, routine_slot{current_ptr_, events_.size() -1});
}

size_t routine::commit_event_round() {
  status_ = routine_status::wait_events;
  thread_->context() = jump_fcontext(thread_->context().fctx, nullptr);
  return happened_index_;
}

void routine::cancel_event_round() {
  // Invalidate other events
  for (auto& other : events_) {
    switch (other.type) {
      case event_type::none:
        break;
      case event_type::timer: {
        auto& data = other.data.get<routine_timer_event_data>();
        --data.neighbor_timers->nb_active;
      } break;
      case event_type::io_read:
        --thread_->nb_suspended_routines_;
        break;
      case event_type::io_write:
        --thread_->nb_suspended_routines_;
        break;
      case event_type::sema_wait: {
        --thread_->nb_suspended_routines_;
        // Remove it from the queue in which it is stored
        auto sema = other.data.get<routine_sema_event_data>().sema;
        if (sema->free(other.data.get<routine_sema_event_data>().index)) {
          // If was in the queue, then free the thread slot
          thread_->unregister_expired_slot(other.data.get<routine_sema_event_data>().slot_index);
        }
      } break;
      case event_type::sema_closed:
      case event_type::io_read_panic:
      case event_type::io_write_panic:
        assert(false);
        break;
    }
  }
  current_ptr_->release();
  current_ptr_.invalidate_all();
  events_.clear();
  //std::swap(previous_events_, events_);
}

void routine::set_as_semaphore_event_candidate(std::size_t index) {
  status_ = routine_status::sema_event_candidate;
  thread_->scheduled_routines_.emplace_back(routine_slot{current_ptr_,index});
}

bool routine::event_happened(std::size_t index, event_status status) {
  auto& event =  events_[index];
  switch (event.type) {
    case event_type::none:
      break;
    case event_type::timer:
      happened_type_ = event_type::timer;
      break;
    case event_type::io_read:
      happened_type_ = event_status::ok == status ? event_type::io_read : event_type::io_read_panic;
      --thread_->nb_suspended_routines_;
      break;
    case event_type::io_write:
      happened_type_ = event_status::ok == status ? event_type::io_write : event_type::io_write_panic;
      --thread_->nb_suspended_routines_;
      break;
    case event_type::sema_wait: {
      --thread_->nb_suspended_routines_;
      auto sema = event.data.get<routine_sema_event_data>().sema;
      int result = sema->counter_.fetch_sub(1,std::memory_order_acquire);
      if (semaphore::disabling_threshold < result) {
        sema->counter_.fetch_add(1,std::memory_order_relaxed);
        happened_type_ = event_type::sema_closed;
      }
      else if (result <= 0) {
        // Failed candidacy
        // Do not invalidate pointers of other events
        // Do not change the routine status
        // Do not invalidate event slot in the thread
        auto slot_index =
            thread_->register_semaphore_wait(routine_slot{current_ptr_, index});
        event.data.get<routine_sema_event_data>().index = sema->write(thread_, slot_index);
        result = sema->counter_.fetch_add(1, std::memory_order_release);
        if (0 <= result) {
          sema->pop_a_waiter(thread_);
        }
        return false;
      }
      else {
        happened_type_ = event_type::sema_wait;
      }
    } break;
    case event_type::sema_closed:
    case event_type::io_read_panic:
    case event_type::io_write_panic:
      assert(false);
      break;
  }

  // Invalidate other events
  for (auto& other : events_) {
    if (&other != &event) {
      switch (other.type) {
        case event_type::none:
          break;
        case event_type::timer: {
            auto& data = other.data.get<routine_timer_event_data>();
            --data.neighbor_timers->nb_active;
          }
          break;
        case event_type::io_read:
          --thread_->nb_suspended_routines_;
          break;
        case event_type::io_write:
          --thread_->nb_suspended_routines_;
          break;
        case event_type::sema_wait: {
          --thread_->nb_suspended_routines_;
          // Remove it from the queue in which it is stored
          auto sema = other.data.get<routine_sema_event_data>().sema;
          if (sema->free(other.data.get<routine_sema_event_data>().index)) {
            // If was in the queue, then free the thread slot
            thread_->unregister_expired_slot(other.data.get<routine_sema_event_data>().slot_index);
          }
        } break;
        case event_type::sema_closed:
        case event_type::io_read_panic:
        case event_type::io_write_panic:
          assert(false);
          break;
      }
    }
  }

  if (happened_type_ == event_type::sema_wait || happened_type_ == event_type::sema_closed) {
    status_ = routine_status::yielding;
    current_ptr_->release();  // In this particular case, the scheduler gets back routine ownership
    current_ptr_.invalidate_all();
    happened_index_ = index;
    return true;
  }
  else if (happened_type_ != event_type::none) {
    thread_->scheduled_routines_.emplace_back(routine_slot{routine_local_ptr_t(std::unique_ptr<routine>(current_ptr_->release())),0});
    current_ptr_.invalidate_all();
    status_ = routine_status::yielding;
    happened_index_ = index;
    return true;
  }

  return false;
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
    default:
      // Not supposed to happen
      assert(false);
      break;
  }

  // Manage the queue requests
  while (context_.data && status_ == routine_status::running) {
    in_context_function* func = static_cast<in_context_function*>(context_.data);
    (*func)(thread_);
    context_ = jump_fcontext(context_.fctx, nullptr);
  }
}

std::size_t routine::get_stack_offset(void* pointer) {
  return  reinterpret_cast<char*>(stack_.sp) - reinterpret_cast<char*>(pointer);
}

}  // namespace internal

}  // namespace boson
