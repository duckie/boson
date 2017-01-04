#include "boson/queues/simple.h"

namespace boson {
namespace queues {

simple_void_queue::simple_void_queue(int nprocs_) {
}

void simple_void_queue::write(std::size_t proc_id, void* data) {
  std::lock_guard<std::mutex> guard(mut_);
  queue_.push_back(data);
}

void* simple_void_queue::read(std::size_t proc_id) {
  std::lock_guard<std::mutex> guard(mut_);
  void* data = nullptr;
  if (!queue_.empty()) {
    data = queue_.front();
    queue_.pop_front();
  }
  return data;
}
}
}
