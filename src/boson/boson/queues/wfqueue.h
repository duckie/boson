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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
#include "wfqueue/lcrq.h"
//#include "wfqueue/wfqueue.h"
#pragma GCC diagnostic pop

extern "C" void queue_init(queue_t * q, int nprocs);
extern "C" void queue_register(queue_t * q, handle_t * th, int id);
extern "C" void enqueue(queue_t * q, handle_t * th, void * v);
extern "C" void * dequeue(queue_t * q, handle_t * th);
extern "C" void queue_free(queue_t * q, handle_t * h);

namespace boson {
namespace queues {


/**
 * Wfqueue is a wait-free MPMC concurrent queue
 *
 * @see https://github.com/chaoran/fast-wait-free-queue
 *
 * Algorithm from Chaoran Yang and John Mellor-Crummey
 * Currently, thei algorithms bugs here so we use LCRQ
 */
template <class ContentType>
class alignas(2 * CACHE_LINE_SIZE) wfqueue {
  using content_t = ContentType;
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

 public:
  using content_type = ContentType;

  class queue_handler {
    wfqueue& parent_;
    handle_t* handler_;

  };

  wfqueue(int nprocs) noexcept(std::is_nothrow_default_constructible<ContentType>()) : nprocs_(nprocs) {
    queue_ = static_cast<queue_t*>(align_malloc(PAGE_SIZE, sizeof(queue_t)));
    queue_init(queue_, nprocs);  // We add 1 proc to be used at destruction to maje sure the queue is empty
    hds_ = static_cast<handle_t**>(align_malloc(PAGE_SIZE, sizeof(handle_t*[nprocs])));;
    for(std::size_t index = 0; index < nprocs; ++index)
      hds_[index] = nullptr; 
    for(std::size_t index = 0; index < nprocs; ++index)
      get_handle(index);
  }

  wfqueue(wfqueue const&) = delete;
  wfqueue(wfqueue&&) = default;
  wfqueue& operator=(wfqueue const&) = delete;
  wfqueue& operator=(wfqueue&&) = default;

  ~wfqueue() {
    // Create one handler to force memory reclamation algorithm
    //enqueue(queue_, get_handle(nprocs_-1), nullptr);
    void* data = nullptr;
    //while(reinterpret_cast<void*>(0xffffffffffffffff) != (data = dequeue(queue_, get_handle(nprocs_))))
    //while(nullptr != (data = dequeue(queue_, get_handle(2*nprocs_))))
    {
      //delete static_cast<ContentType>(data);
    }
    for (int index = 0; index < nprocs_; ++index) {
      if (hds_[index])
        queue_free(queue_, hds_[index]);
      //free(hds_[index]);
    }
    free(queue_);
    free(hds_);
  }
  
  template <class ... Args>
  void push(std::size_t proc_id, Args&& ... args) {
    assert(proc_id < nprocs_);
    enqueue(queue_, get_handle(proc_id), static_cast<void*>(ContentType(std::forward<Args>(args)...)));
  }

  inline ContentType pop(std::size_t proc_id) {
    assert(proc_id < nprocs_);
    void* result = dequeue(queue_, get_handle(proc_id));
    return reinterpret_cast<void*>(0xffffffffffffffff) == result ? nullptr : static_cast<ContentType>(result);
    //return nullptr == result ? nullptr : static_cast<ContentType>(result);
  }
};

};  // namespace queues
};  // namespace boson

#endif  // BOSON_QUEUES_WFQUEUE_H_
