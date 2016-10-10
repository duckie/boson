#ifndef BOSON_QUEUES_SIMPLE_QUEUE_H_
#define BOSON_QUEUES_SIMPLE_QUEUE_H_

#include <list>
#include <cstdint>
#include <mutex>

namespace boson {
namespace queues {

class simple_queue {
  std::list<void*> queue_;
  std::mutex mut_;

 public:
  simple_queue(int nprocs_);
  simple_queue(simple_queue const&) = delete;
  simple_queue(simple_queue&&) = default;
  simple_queue& operator=(simple_queue const&) = delete;
  simple_queue& operator=(simple_queue&&) = default;
  void push(std::size_t proc_id, void* data);
  void* pop(std::size_t proc_id);
};

}
}

#endif  // BOSON_QUEUES_SIMPLE_QUEUE_H_
