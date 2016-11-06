#ifndef BOSON_CHANNEL_H_
#define BOSON_CHANNEL_H_

#include <list>
#include <memory>
#include <mutex>
#include "boson/semaphore.h"
#include "engine.h"
#include "internal/routine.h"
#include "internal/thread.h"

namespace boson {

enum class channel_result_value { ok, timedout, closed };

struct channel_result {
  channel_result_value value;

  inline operator bool () const {
    return value == channel_result_value::ok;
  };

  inline operator channel_result_value () const {
    return value;
  }
};

template <class ContentType, std::size_t Size>
class channel_impl {
  static_assert(0 < Size, "Boson channels do not support zero size.");
  template <class Content, std::size_t InSize, class Func>
  friend class event_channel_read_storage;
  template <class Content, std::size_t InSize, class Func>
  friend class event_channel_write_storage;

  std::array<ContentType, Size> buffer_;
  std::atomic<size_t> head_;
  std::atomic<size_t> tail_;

  // Waiting lists
  boson::shared_semaphore readers_slots_;
  boson::shared_semaphore writer_slots_;

 public:
  channel_impl() : buffer_{}, head_{0}, tail_{0}, readers_slots_(0), writer_slots_(Size) {
  }

  ~channel_impl() {
    // delete queue_;
  }

  inline void close() {
    writer_slots_.disable();
    if (head_.load(std::memory_order_acquire) - tail_.load(std::memory_order_acquire) == 0)
      readers_slots_.disable();
  }

  void consume_write(thread_id, ContentType value) {
    size_t head = head_.fetch_add(1, std::memory_order_acq_rel);
    buffer_[head % Size] = std::move(value);
    readers_slots_.post();
  }

  void consume_read(thread_id, ContentType& value) {
    size_t tail = tail_.fetch_add(1, std::memory_order_acq_rel);
    value = std::move(buffer_[tail % Size]);
    auto rc = writer_slots_.post();
    if (!rc) { // Channel has been closed !
      if (head_.load(std::memory_order_acquire) - tail_.load(std::memory_order_acquire) == 0) // All elements are consumed
        readers_slots_.disable();
    }
  }

  /**
   * Write an element in the channel
   *
   * Returns false only if the channel is closed.
   */
  channel_result write(thread_id tid, ContentType value, int timeout_ms = -1) {
    auto ticket = writer_slots_.wait(timeout_ms = -1);
    if (!ticket)
      return {ticket == semaphore_return_value::timedout ? channel_result_value::timedout
                                                         : channel_result_value::closed};
    consume_write(tid, value);
    return { channel_result_value::ok };
  }

  channel_result read(thread_id tid, ContentType& value, int  timeout_ms = -1) {
    auto ticket = readers_slots_.wait(timeout_ms);
    if (!ticket) {
      return {ticket == semaphore_return_value::timedout ? channel_result_value::timedout
                                                         : channel_result_value::closed};

    }
    consume_read(tid, value);
    return { channel_result_value::ok };
  }
};

/**
 * Specialization for the channel containing nothing
 */
template <std::size_t Size>
class channel_impl<std::nullptr_t,Size> {
  static_assert(0 < Size, "Boson channels do not support zero size.");
  template <class Content, std::size_t InSize, class Func>
  friend class event_channel_read_storage;
  template <class Content, std::size_t InSize, class Func>
  friend class event_channel_write_storage;

  using ContentType = std::nullptr_t;

  // Waiting lists
  boson::shared_semaphore readers_slots_;
  boson::shared_semaphore writer_slots_;

 public:
  channel_impl() : readers_slots_(0), writer_slots_(Size) {
  }

  ~channel_impl() {
  }

  inline void close() {
    writer_slots_.disable();
  }

  void consume_write(thread_id tid, ContentType value) {
    readers_slots_.post();
  }

  void consume_read(thread_id tid, ContentType& value) {
    value = nullptr;
    writer_slots_.post();
  }

  /**
   * Write an element in the channel
   *
   * Returns false only if the channel is closed.
   */
  channel_result write(thread_id tid, ContentType value, int timeout_ms = -1) {
    auto ticket = writer_slots_.wait(timeout_ms = -1);
    if (!ticket)
      return {ticket == semaphore_return_value::timedout ? channel_result_value::timedout
                                                         : channel_result_value::closed};
    consume_write(tid, value);
    return { channel_result_value::ok };
  }

  channel_result read(thread_id tid, ContentType& value, int  timeout_ms = -1) {
    auto ticket = readers_slots_.wait(timeout_ms);
    if (!ticket)
      return {ticket == semaphore_return_value::timedout ? channel_result_value::timedout
                                                         : channel_result_value::closed};
    consume_read(tid, value);
    return { channel_result_value::ok };
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
  template <class Content, std::size_t InSize, class Func>
  friend class event_channel_read_storage;
  template <class Content, std::size_t InSize, class Func>
  friend class event_channel_write_storage;
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

  //template <class... Args>
  //inline bool write(Args&&... args) {
    //return channel_->write(get_id(), std::forward<Args>(args)...);
  //}
  inline void close() {
    channel_->close();
  }

  inline void consume_write(ContentType value) {
    channel_->consume_write(get_id(), std::move(value));
  }

  inline void consume_read(ContentType& value) {
    channel_->consume_read(get_id(), value);
  }

  inline channel_result write(ContentType value, int timeout_ms = -1) {
    return channel_->write(get_id(), std::move(value), timeout_ms);
  }

  inline channel_result read(ContentType& value, int timeout_ms = -1) {
    return channel_->read(get_id(), value, timeout_ms);
  }
};

template <class ContentType, std::size_t Size, class ValueType>
inline auto operator << (channel<ContentType, Size>& channel, ValueType&& value) 
-> typename std::enable_if<std::is_convertible<ValueType,ContentType>::value, channel_result>::type
{
  return channel.write(static_cast<ContentType>(std::forward<ValueType>(value)));
}

template <class ContentType, std::size_t Size>
inline channel_result operator >> (channel<ContentType, Size>& channel, ContentType& value) 
{
  return channel.read(value);
}

}  // namespace bosn

#endif  // BOSON_CHANNEL_H_
