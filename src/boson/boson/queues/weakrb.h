#ifndef BOSON_QUEUES_WEAKRB_H_
#define BOSON_QUEUES_WEAKRB_H_
#include <atomic>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace boson {
namespace queues {

/**
 * WeakRB is an efficient SPSC bounded queue
 *
 * Also konwn as a ring buffer. Algorithm by
 * Nhat Minh Le, Adrien Guatto, Albert Cohen, Antoniu Pop
 * TODO: add url of the pdf
 *
 */
template <class ContentType>
class weakrb {
  using content_t = ContentType;
  using content_array_t = std::vector<ContentType>;
  using index_t = std::atomic<std::size_t>;

#if defined(BOSON_USE_VALGRIND)
  // Does not work
  static constexpr std::memory_order const order_relaxed = std::memory_order::memory_order_relaxed;
  static constexpr std::memory_order const order_acquire = std::memory_order::memory_order_acquire;
  static constexpr std::memory_order const order_release = std::memory_order::memory_order_release;
#else
  static constexpr std::memory_order const order_relaxed = std::memory_order::memory_order_relaxed;
  static constexpr std::memory_order const order_acquire = std::memory_order::memory_order_acquire;
  static constexpr std::memory_order const order_release = std::memory_order::memory_order_release;
#endif

  index_t front_ = {0};
  std::size_t pfront_ = {0};
  index_t back_ = {0};
  std::size_t cback_ = {0};
  size_t const size_;
  content_array_t data_;

 public:
  using content_type = ContentType;

  weakrb(size_t capacity = 1) noexcept(std::is_nothrow_default_constructible<ContentType>())
      : size_{capacity}, data_(capacity) {
  }
  weakrb(weakrb const&) = delete;
  weakrb(weakrb&&) noexcept(false) = default;
  weakrb& operator=(weakrb const&) = delete;
  weakrb& operator=(weakrb&&) noexcept(false) = default;
  ~weakrb() = default;

  template <class... Args>
  bool push(Args&&... args) {
    size_t back, front;
    back = back_.load(order_relaxed);
    if (pfront_ + size_ - back < 1) {
      pfront_ = front_.load(order_acquire);
      if (pfront_ + size_ - back < 1) return false;
    }
    data_[back % size_] = content_t(std::forward<Args>(args)...);
    back_.store(back + 1, order_release);
    return true;
  };

  bool pop(content_type& element) {
    size_t back, front;
    front = front_.load(std::memory_order::memory_order_relaxed);
    if (cback_ - front < 1) {
      cback_ = back_.load(order_acquire);
      if (cback_ - front < 1) return false;
    }
    element = std::move(data_[front % size_]);
    front_.store(front + 1, order_release);
    return true;
  };
};

};  // namespace queues
};  // namespace boson

#endif  // BOSON_QUEUES_WEAKRB_H_
