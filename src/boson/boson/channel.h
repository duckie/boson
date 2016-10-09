#ifndef BOSON_CHANNEL_H_
#define BOSON_CHANNEL_H_

#include <list>
#include <memory>
#include <mutex>
#include "boson/semaphore.h"
#include "internal/routine.h"
#include "internal/thread.h"
#include "queues/wfqueue.h"
#include "engine.h"

namespace boson {

template <class ContentType, std::size_t Size>
class channel_impl {
  std::array<ContentType,Size> buffer_;
  std::atomic<size_t> head_;
  std::atomic<size_t> tail_;

  // Waiting lists
  boson::semaphore readers_slots_;
  boson::semaphore writer_slots_;

 public:
  channel_impl() : buffer_{}, head_{0}, tail_{0}, readers_slots_(0), writer_slots_(Size) {
  }

  ~channel_impl() {
    //delete queue_;
  }

  /**
   * Write an element in the channel
   *
   * Returns false only if the channel is closed.
   */
  template <class... Args>
  bool push(thread_id tid, Args&&... args) {
    writer_slots_.wait();
    size_t head = head_.fetch_add(1,std::memory_order_acq_rel);
    buffer_[head % Size] = ContentType(std::forward<Args>(args)...);
    readers_slots_.post();
    return true;
  }

  bool pop(thread_id tid, ContentType& value) {
    readers_slots_.wait();
    size_t tail = tail_.fetch_add(1,std::memory_order_acq_rel);
    value = std::move(buffer_[tail % Size]);
    writer_slots_.post();
    return true;
  }
};

/**
 * Specialization for the sync channel
 */
template <class ContentType>
class channel_impl<ContentType, 0> {
  using queue_t = queues::base_wfqueue;
  ContentType buffer_;
  boson::semaphore readers_slots_;
  boson::semaphore writer_slots_;

 public:
  channel_impl() : readers_slots_(0), writer_slots_(1) {
  }

  ~channel_impl() {
    //delete queue_;
  }

  /**
   * Write an element in the channel
   *
   * Returns false only if the channel is closed.
   */
  template <class... Args>
  bool push(thread_id tid, Args&&... args) {
    writer_slots_.wait();
    buffer_ = ContentType(std::forward<Args>(args)...);
    readers_slots_.post();
    return true;
  }

  bool pop(thread_id tid, ContentType& value) {
    readers_slots_.wait();
    value = std::move(buffer_);
    writer_slots_.post();
    return true;
  }
};

/**
 * Specialization for the channel containing nothing
 */
template <std::size_t Size>
class channel_impl<std::nullptr_t, Size> {
  boson::semaphore semaphore_;

 public:
  channel_impl() : semaphore_{Size} {
  }

  ~channel_impl() {
  }

  /**
   * Write an element in the channel
   *
   * Returns false only if the channel is closed.
   */
  template <class... Args>
  bool push(thread_id tid, Args&&... args) {
    semaphore_.post();
    return true;
  }

  bool pop(thread_id tid, std::nullptr_t& value) {
    semaphore_.wait();
    value = nullptr;
    return true;
  }
};

/**
 * Channel use interface
 *
 * The user is supposed ot use and copy channel objects
 * and not directly use the implementation. channel objects must
 * never be transmitted to new routines through reference
 * but only by copy.
 */
template <class ContentType, std::size_t Size>
class channel {
  using value_t = ContentType;
  using impl_t = channel_impl<value_t, Size>;

  std::shared_ptr<impl_t> channel_;
  thread_id thread_id_{0};

  thread_id get_id() {
    if (!thread_id_) {
      internal::thread* this_thread = internal::current_thread();
      assert(this_thread);
      thread_id_ = this_thread->id();
    }
    return thread_id_;
  }

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
  channel(channel const&) = default;
  channel(channel&&) = default;
  channel& operator=(channel const&) = default;
  channel& operator=(channel&&) = default;

  template <class... Args>
  inline bool push(Args&&... args) {
    return channel_->push(get_id(), std::forward<Args>(args)...);
  }

  inline bool pop(ContentType& value) {
    return channel_->pop(get_id(), value);
  }
};

// TODO: add specialization for channel<std::nullptr_t>

}  // namespace bosn

#endif  // BOSON_CHANNEL_H_
