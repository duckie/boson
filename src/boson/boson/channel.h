#ifndef BOSON_CHANNEL_H_
#define BOSON_CHANNEL_H_

#include <memory>
#include "internal/routine.h"
#include "internal/thread.h"
#include "queues/mpmc.h"

namespace boson {

template <class ContentType>
class channel_impl {
  using routine_ptr_t = std::unique_ptr<internal::routine>;

  queues::bounded_mpmc<ContentType> queue_;
  queues::unbounded_mpmc<routine_ptr_t> listeners_;
  queues::unbounded_mpmc<routine_ptr_t> writers_;

 public:
  channel_impl(size_t capacity)
      : queue_(0 == capacity ? 1 : capacity),
        listeners_(1),  // TODO: use nb of threads
        writers_(1)     // TODO: use nb of threads
  {
  }

  /**
   * Write an element in the channel
   *
   * Returns false only if the channel is closed.
   */
  template <class... Args>
  bool push(Args&&... args) {
    bool success = queue_.write(std::forward<Args>(args)...);
    if (!success) {
      // internal::transfer_t& main_context = internal::current_thread_context;
      // internal::routine* current_routine = static_cast<internal::routine*>(main_context.data);
      // current_routine->status_ = internal::routine_status::wait_channel_write;
      // writers_.push(current_routine);
      //
      // main_context = jump_fcontext(main_context.fctx, nullptr);
      // current_routine->previous_status_ = routine_status::wait_channel_write;
      // current_routine->status_ = routine_status::running;
    }
  };
};

/**
 * Channel use interface
 *
 * The user is supposed ot use and copy channel objects
 * and not directly use the implementation. channel objects must
 * never be transmitted to new routines through reference
 * but onl by copy.
 */
template <class ContentType>
class channel {
  using value_t = ContentType;
  using impl_t = channel_impl<value_t>;

  std::shared_ptr<impl_t> channel_;
  size_t capacity;

 public:
  using value_type = ContentType;

  /**
   * Channel construction determines its behavior
   *
   * == 0 means sync channel
   * > 0 means channel of size capacity
   */
  channel(size_t capacity = 0) : channel_{new impl_t(capacity)} {
  }
};

// TODO: add specialization for channel<std::nullptr_t>

}  // namespace bosn

#endif  // BOSON_CHANNEL_H_
