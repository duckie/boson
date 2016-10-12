#include "boson/queues/lcrq.h"
#include <set>

namespace boson {
namespace queues {

int lcrq::htable_insert(void **tbl, size_t size, void *ptr) {
  int index = XXH32(ptr, 1, 0) % size;
  int i;

  for (i = index; i < size; ++i) {
    if (tbl[i] == NULL) {
      tbl[i] = ptr;
      return 0;
    }
  }

  for (i = 0; i < index; ++i) {
    if (tbl[i] == NULL) {
      tbl[i] = ptr;
      return 0;
    }
  }

  return -1;
}

int lcrq::htable_lookup(void **tbl, size_t size, void *ptr) {
  int index = XXH32(ptr, 1, 0) % size;
  int i;

  for (i = index; i < size; ++i) {
    if (tbl[i] == ptr) {
      return 1;
    } else if (tbl[i] == NULL) {
      return 0;
    }
  }

  for (i = 0; i < index; ++i) {
    if (tbl[i] == ptr) {
      return 1;
    } else if (tbl[i] == NULL) {
      return 0;
    }
  }

  return 0;
}

void lcrq::hzdptr_init(hzdptr_t *hzd, int nprocs, int nptrs) {
  hzd->nprocs = nprocs;
  hzd->nptrs = nptrs;
  hzd->nretired = 0;
  hzd->ptrs = (void **)calloc(hzdptr_size(nprocs, nptrs), 1);

  _hzdptr_enlist(hzd);
}

void lcrq::_hzdptr_retire(hzdptr_t *hzd, void **rlist) {
  size_t size = HZDPTR_HTBL_SIZE(hzd->nprocs, hzd->nptrs);
  void *plist[size];
  memset(plist, 0, sizeof(plist));

  hzdptr_t *me = hzd;
  void *ptr;

  while ((hzd = hzd->next) != me) {
    int i;
    for (i = 0; i < hzd->nptrs; ++i) {
      ptr = hzd->ptrs[i];

      if (ptr != NULL) {
        htable_insert(plist, size, ptr);
      }
    }
  }

  int nretired = 0;

  /** Check pointers in retire list with plist. */
  int i;
  for (i = 0; i < hzd->nretired; ++i) {
    ptr = rlist[i];

    if (htable_lookup(plist, size, ptr)) {
      rlist[nretired++] = ptr;
    } else {
      free(ptr);
    }
  }

  hzd->nretired = nretired;
}

void lcrq::hzdptr_exit(hzdptr_t *hzd) {
  int i;
  void **rlist = &hzd->ptrs[hzd->nptrs];

  for (i = 0; i < hzd->nretired; ++i) {
    free(rlist[i]);
  }

  hzd->nretired = 0;
  hzd->next = hzd;
}

auto lcrq::get_handle(std::size_t proc_id) -> handle_t * {
  handle_t *hd = hds_[proc_id];
  if (hd == nullptr) {
    // This handler has not been initialized yet
    hd = hds_[proc_id] = static_cast<handle_t *>(align_malloc(PAGE_SIZE, sizeof(handle_t)));
    ;
    memset(static_cast<void *>(hd), 0, sizeof(handle_t));
    queue_register(queue_, hd, proc_id);
  }
  return hd;
}
lcrq::lcrq(int nprocs) : nprocs_{nprocs} {
  queue_ = static_cast<queue_t *>(align_malloc(PAGE_SIZE, sizeof(queue_t)));
  queue_init(queue_,
             nprocs);  // We add 1 proc to be used at destruction to maje sure the queue is empty
  hds_ = static_cast<handle_t **>(align_malloc(PAGE_SIZE, sizeof(handle_t * [nprocs + 1])));
  ;
  for (std::size_t index = 0; index < nprocs + 1; ++index) hds_[index] = nullptr;
}
lcrq::~lcrq() {
  // Create one handler to force memory reclamation algorithm
  enqueue(queue_, get_handle(nprocs_), nullptr);
  void *data = nullptr;
  while (reinterpret_cast<void *>(0xffffffffffffffff) !=
         (data = dequeue(queue_, get_handle(nprocs_))))
    ;

  // Empty hazard pointers
  std::set<void *> already_freed;
  
  // Freeing RingQueues
  std::set<void *> to_free;
  RingQueue* rq = queue_->head;
  //RingQueue* iq = queue_->head;
  //while(rq != nullptr) {
    ////if (std::end(already_freed) == already_freed.find(rq)) {
    //auto nrq = rq->next;
    //to_free.insert(rq);
      ////free(rq);
      ////if (nrq == iq)
        ////break;
    ////}
    //rq = nrq;
  //}
//
  //rq = queue_->tail;
  ////RingQueue* iq = queue_->head;
  //while(rq != nullptr) {
    ////if (std::end(already_freed) == already_freed.find(rq)) {
    //auto nrq = rq->next;
    //to_free.insert(rq);
      ////free(rq);
      ////if (nrq == iq)
        ////break;
    ////}
    //rq = nrq;
  //}

  //rq = queue_->tail;
  //iq = queue_->tail;
  //while(rq != nullptr) {
    //RingQueue* nrq = nullptr;
    //if (std::end(already_freed) == already_freed.find(rq)) {
      //nrq = rq->next;
      //already_freed.insert(rq);
      //free(rq);
      //if (nrq == iq)
        //break;
    //}
    //rq = nrq;
  //}
  
  // Clean tail
  auto init_tail = _tail;
  auto tail = _tail;
  while (tail) {
    auto next_tail = tail->next;
    if (init_tail == next_tail)
      break;
    for(int i=0; i < tail->nptrs; ++i)
      to_free.insert(tail->ptrs[i]);
    to_free.insert(tail->ptrs);

    //if (std::end(already_freed) == already_freed.find(tail->ptrs)) {
      //if (std::end(already_freed) == already_freed.find(tail->ptrs[0])) {
        //already_freed.insert(tail->ptrs[0]);
        //free(tail->ptrs[0]);
      //}
      //already_freed.insert(tail->ptrs);
      //free(tail->ptrs);
    //}
    tail = next_tail;
  }
  //to_free.insert(_tail);

  // Empty local data
  for (int index = 0; index < nprocs_ + 1; ++index) {
    if (hds_[index]) {
      to_free.insert(hds_[index]->hzdptr.ptrs);
      for (int i=0; i < hds_[index]->hzdptr.nptrs; ++i)
        to_free.insert(hds_[index]->hzdptr.ptrs[i]);
      to_free.insert(hds_[index]);
      //if (std::end(already_freed) == already_freed.find(hds_[index]->hzdptr.ptrs)) {
        //if (std::end(already_freed) == already_freed.find(hds_[index]->hzdptr.ptrs[0])) {
          //already_freed.insert(hds_[index]->hzdptr.ptrs[0]);
          //free(hds_[index]->hzdptr.ptrs[0]);
        //}
        //already_freed.insert(hds_[index]->hzdptr.ptrs);
        //free(hds_[index]->hzdptr.ptrs);
      //}
      //free(hds_[index]);
    }
  }

  for (void* ptr : to_free) {
    free(ptr);
  }


  free(queue_);
  free(hds_);
}
void lcrq::write(std::size_t proc_id, void *data) {
  assert(proc_id < nprocs_);
  enqueue(queue_, get_handle(proc_id), data);
}

void *lcrq::read(std::size_t proc_id) {
  assert(proc_id < nprocs_);
  void *result = dequeue(queue_, get_handle(proc_id));
  return reinterpret_cast<void *>(0xffffffffffffffff) == result ? nullptr : result;
}
}
}
