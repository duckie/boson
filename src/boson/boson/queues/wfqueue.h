#ifndef BOSON_QUEUES_WFQUEUE_H_
#define BOSON_QUEUES_WFQUEUE_H_
#include <unistd.h>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <utility>
#include <list>
#include <mutex>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
extern "C" {
#include "wfqueue/queue.h"
}
#pragma GCC diagnostic pop

namespace boson {
namespace queues {

class simple_wfqueue {
  std::list<void*> queue_;
  std::mutex mut_;
 public:
  simple_wfqueue(int nprocs_);
  simple_wfqueue(simple_wfqueue const&) = delete;
  simple_wfqueue(simple_wfqueue&&) = default;
  simple_wfqueue& operator=(simple_wfqueue const&) = delete;
  simple_wfqueue& operator=(simple_wfqueue&&) = default;
  void push(std::size_t proc_id, void* data);
  void* pop(std::size_t proc_id);
};

/**
 * Wfqueue is a wait-free MPMC concurrent queue
 *
 * @see https://github.com/chaoran/fast-wait-free-queue
 *
 * Algorithm from Chaoran Yang and John Mellor-Crummey
 * Currently, thei algorithms bugs here so we use LCRQ
 */
class base_wfqueue {
 public:
  queue_t* queue_;
  handle_t** hds_;
  int nprocs_;

  inline handle_t* get_handle(std::size_t proc_id) {
    handle_t* hd = hds_[proc_id];
    if (hd == nullptr) {
      // This handler has not been initialized yet
      hd = hds_[proc_id] = static_cast<handle_t*>(align_malloc(PAGE_SIZE, sizeof(handle_t)));;
      memset(static_cast<void*>(hd),0,sizeof(handle_t));
      queue_register(queue_, hd, proc_id);
    }
    return hd;
  }

  base_wfqueue(int nprocs) : nprocs_{nprocs} {
    queue_ = static_cast<queue_t*>(align_malloc(PAGE_SIZE, sizeof(queue_t)));
    queue_init(queue_, nprocs);  // We add 1 proc to be used at destruction to maje sure the queue is empty
    hds_ = static_cast<handle_t**>(align_malloc(PAGE_SIZE, sizeof(handle_t*[nprocs+1])));;
    for(std::size_t index = 0; index < nprocs+1; ++index)
      hds_[index] = nullptr; 
    //for(std::size_t index = 0; index < nprocs; ++index)
      //get_handle(index);
  }

  base_wfqueue(base_wfqueue const&) = delete;
  base_wfqueue(base_wfqueue&&) = default;
  base_wfqueue& operator=(base_wfqueue const&) = delete;
  base_wfqueue& operator=(base_wfqueue&&) = default;

  ~base_wfqueue() {
    // Create one handler to force memory reclamation algorithm
    enqueue(queue_, get_handle(nprocs_), nullptr);
    //while(reinterpret_cast<void*>(0xffffffffffffffff) != (data = dequeue(queue_, get_handle(nprocs_))))
    void* data = nullptr;
    while(nullptr != (data = dequeue(queue_, get_handle(nprocs_))))
    {
      //delete static_cast<ContentType>(data);
    }
    for (int index = 0; index < nprocs_; ++index) {
      //if (hds_[index])
        //queue_free(queue_, hds_[index]);
      //free(hds_[index]);
    }
    free(queue_);
    free(hds_);
  }
  
  void push(std::size_t proc_id, void* data) {
    assert(proc_id < nprocs_);
    enqueue(queue_, get_handle(proc_id), data);
  }

  void* pop(std::size_t proc_id) {
    assert(proc_id < nprocs_);
    void* result = dequeue(queue_, get_handle(proc_id));
    //return reinterpret_cast<void*>(0xffffffffffffffff) == result ? nullptr : static_cast<ContentType>(result);
    return result;
    //return nullptr == result ? nullptr : static_cast<ContentType>(result);
  }
};

template <class ContentType>
class wfqueue : public base_wfqueue {
 public:
  using base_wfqueue::base_wfqueue;
};

};  // namespace queues
};  // namespace boson

#endif  // BOSON_QUEUES_WFQUEUE_H_
