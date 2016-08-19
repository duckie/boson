#ifndef BOSON_QUEUES_WEAKRB_H_
#define BOSON_QUEUES_WEAKRB_H_
#include <cstdint>
#include <atomic>

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
template <class ContentType, std::size_t Size = 1>
class weakrb {
  using content_t = ContentType;
  using content_array_t = std::array<ContentType, Size>;
  using index_t = std::atomic<std::size_t>;

  index_t front_ = {0};
  std::size_t pfront_ = {0};
  index_t back_ = {0};
  std::size_t cback_ = {0};
  content_array_t data_ = {};

 public:
  using content_type = ContentType;
  static constexpr std::size_t const size = Size;

  bool push(content_type& element) {
    size_t back, front;
    back = back_.load(std::memory_order::memory_order_relaxed);
    if (pfront_ + Size - back < 1) {
      pfront_ = front_.load(std::memory_order::memory_order_acquire);
      if (pfront_ + Size - back < 1) return false;
    }
    data_[back % Size] = std::move(element);
    back_.store(back + 1, std::memory_order::memory_order_release);
    return true;
  };

  bool pop(content_type& element) {
    size_t back, front;
    front = front_.load(std::memory_order::memory_order_relaxed);
    if (cback_ - front < 1) {
      cback_ = back_.load(std::memory_order::memory_order_acquire);
      if (cback_ - front < 1) return false;
    }
    element = std::move(data_[front % Size]);
    front_.store(front + 1, std::memory_order::memory_order_release);
    return true;
  };
};

};  // namespace queues
};  // namespace boson

#endif  // BOSON_QUEUES_WEAKRB_H_
