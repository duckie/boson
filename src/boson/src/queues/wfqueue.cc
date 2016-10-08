#include "boson/queues/wfqueue.h"


namespace boson {
namespace queues {

simple_wfqueue::simple_wfqueue(int nprocs_) {
}

  void simple_wfqueue::push(std::size_t proc_id, void* data) {
    std::lock_guard<std::mutex> guard(mut_);
    queue_.push_back(data); 
  }

  void* simple_wfqueue::pop(std::size_t proc_id) {
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
