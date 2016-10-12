#include "boson/queues/wfqueue.h"

namespace boson {
namespace queues {

std::atomic<base_wfqueue::handle_t*> base_wfqueue::_tail{nullptr};

auto base_wfqueue::get_handle(std::size_t proc_id) -> handle_t* {
  handle_t* hd = hds_[proc_id];
  if (hd == nullptr) {
    // This handler has not been initialized yet
    hd = hds_[proc_id] = static_cast<handle_t*>(align_malloc(PAGE_SIZE, sizeof(handle_t)));
    ;
    memset(static_cast<void*>(hd), 0, sizeof(handle_t));
    queue_register(queue_, hd, proc_id);
  }
  return hd;
}
base_wfqueue::base_wfqueue(int nprocs) : nprocs_{nprocs} {
  queue_ = static_cast<queue_t*>(align_malloc(PAGE_SIZE, sizeof(queue_t)));
  queue_init(queue_,
             nprocs);  // We add 1 proc to be used at destruction to maje sure the queue is empty
  hds_ = static_cast<handle_t**>(align_malloc(PAGE_SIZE, sizeof(handle_t * [nprocs + 1])));
  ;
  for (std::size_t index = 0; index < nprocs + 1; ++index) hds_[index] = nullptr;
}
base_wfqueue::~base_wfqueue() {
  // Create one handler to force memory reclamation algorithm
  enqueue(queue_, get_handle(nprocs_), nullptr);
  // while(reinterpret_cast<void*>(0xffffffffffffffff) != (data = dequeue(queue_,
  // get_handle(nprocs_))))
  void* data = nullptr;
  while (nullptr != (data = dequeue(queue_, get_handle(nprocs_))))
    ;
  for (int index = 0; index < nprocs_ + 1; ++index) {
    if (hds_[index]) {
      queue_free(queue_, hds_[index]);
      // free(hds_[index]);
    }
  }
  free(queue_);
  free(hds_);
}
void base_wfqueue::write(std::size_t proc_id, void* data) {
  assert(proc_id < nprocs_);
  enqueue(queue_, get_handle(proc_id), data);
}

void* base_wfqueue::read(std::size_t proc_id) {
  assert(proc_id < nprocs_);
  void* result = dequeue(queue_, get_handle(proc_id));
  // return reinterpret_cast<void*>(0xffffffffffffffff) == result ? nullptr :
  // static_cast<ContentType>(result);
  return result;
}
}
}
