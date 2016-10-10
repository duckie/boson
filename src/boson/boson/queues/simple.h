#ifndef BOSON_QUEUES_SIMPLE_QUEUE_H_
#define BOSON_QUEUES_SIMPLE_QUEUE_H_

#include <deque>
#include <cstdint>
#include <mutex>

namespace boson {
namespace queues {

class simple_queue {
  std::deque<void*> queue_;
  std::mutex mut_;

 public:
  simple_queue(int nprocs_);
  simple_queue(simple_queue const&) = delete;
  simple_queue(simple_queue&&) = default;
  simple_queue& operator=(simple_queue const&) = delete;
  simple_queue& operator=(simple_queue&&) = default;
  void write(std::size_t proc_id, void* data);
  void* read(std::size_t proc_id);
};

}
}

#endif  // BOSON_QUEUES_SIMPLE_QUEUE_H_
