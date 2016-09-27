#ifndef BOSON_CHANNEL_H_

#define BOSON_CHANNEL_H_

#include <list>
#include <memory>
#include <mutex>
#include "internal/routine.h"
#include "internal/thread.h"
#include "queues/mpmc.h"
#include "boson/semaphore.h"

namespace boson {

template <class ContentType, std::size_t Size>
class channel_impl {
  using routine_ptr_t = std::unique_ptr<internal::routine>;
  queues::bounded_mpmc<ContentType> queue_;
  boson::semaphore readers_slots_;
  boson::semaphore writer_slots_;

 public:
  channel_impl() : queue_(Size), readers_slots_(0), writer_slots_(Size) {
  }

  /**
   * Write an element in the channel
   *
   * Returns false only if the channel is closed.
   */
  template <class... Args>
  bool push(Args&&... args) {
    writer_slots_.wait();
    bool success = queue_.push(std::forward<Args>(args)...);
    assert(success);
    readers_slots_.post();
    return true;
  }

  bool pop(ContentType& value) {
    readers_slots_.wait();
    bool success = queue_.pop(value);
    assert(success);
    writer_slots_.post();
    return true;
  }
};

/**
 * Specialization for the sync channel
 */
template <class ContentType>
class channel_impl<ContentType,0> {
  using routine_ptr_t = std::unique_ptr<internal::routine>;
  queues::bounded_mpmc<ContentType> queue_;
  boson::semaphore readers_slots_;
  boson::semaphore writer_slots_;

 public:
  channel_impl() : queue_(1), readers_slots_(0), writer_slots_(1) {
  }

  /**
   * Write an element in the channel
   *
   * Returns false only if the channel is closed.
   */
  template <class... Args>
  bool push(Args&&... args) {
    writer_slots_.wait();
    bool success = queue_.push(std::forward<Args>(args)...);
    assert(success);
    readers_slots_.post();
    return true;
  }

  bool pop(ContentType& value) {
    readers_slots_.wait();
    bool success = queue_.pop(value);
    assert(success);
    writer_slots_.post();
    return true;
  }
};

/**
 * Channel use interface
 *
 * The user is supposed ot use and copy channel objects
 * and not directly use the implementation. channel objects must
 * never be transmitted to new routines through reference
 * but onl by copy.
 */
template <class ContentType, std::size_t Size>
class channel {
  using value_t = ContentType;
  using impl_t = channel_impl<value_t, Size>;

  std::shared_ptr<impl_t> channel_;

 public:
  using value_type = ContentType;
  static constexpr size_t size = Size;

  /**
   * Channel construction determines its behavior
   *
   * == 0 means sync channel
   * > 0 means channel of size capacity
   */
  channel() : channel_{new impl_t} {
  }

  template <class... Args>
  inline bool push(Args&&... args) {
    return channel_->push(std::forward<Args>(args)...);
  }

  inline bool pop(ContentType& value) {
    return channel_->pop(value);
  }
};

// TODO: add specialization for channel<std::nullptr_t>

}  // namespace bosn

#endif  // BOSON_CHANNEL_H_
