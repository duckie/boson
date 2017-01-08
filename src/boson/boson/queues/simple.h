#ifndef BOSON_QUEUES_SIMPLE_QUEUE_H_
#define BOSON_QUEUES_SIMPLE_QUEUE_H_

#include <deque>
#include <cstdint>
#include <mutex>

namespace boson {
namespace queues {

// Same interface as lcrq
class simple_void_queue {
  std::deque<void*> queue_;
  std::mutex mut_;

 public:
  simple_void_queue(int nprocs_);
  simple_void_queue(simple_void_queue const&) = delete;
  simple_void_queue(simple_void_queue&&) = default;
  simple_void_queue& operator=(simple_void_queue const&) = delete;
  simple_void_queue& operator=(simple_void_queue&&) = default;
  void write(std::size_t proc_id, void* data);
  void* read(std::size_t proc_id);
};

// Same interface as mpsc
template <class T>
class simple_queue {
  std::deque<T> queue_;
  std::mutex mut_;

 public:
  simple_queue() {}
  simple_queue(simple_queue const&) = delete;
  simple_queue(simple_queue&&) = default;
  simple_queue& operator=(simple_queue const&) = delete;
  simple_queue& operator=(simple_queue&&) = default;

  void write(T value) 
  {
    std::lock_guard<std::mutex> guard(mut_);
    queue_.push_back(std::move(value));
  }

  bool read(T& value) {
    std::lock_guard<std::mutex> guard(mut_);
    if (!queue_.empty()) {
      value = std::move(queue_.front());
      queue_.pop_front();
      return true;
    }
    return false;
  }
};
}
}

#endif  // BOSON_QUEUES_SIMPLE_QUEUE_H_
