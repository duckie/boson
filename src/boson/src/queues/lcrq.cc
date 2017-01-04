#include "boson/queues/lcrq.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"

extern "C" {
#include "wfqueue/align.h"
#include "wfqueue/primitives.h"
#include "wfqueue/xxhash.h"
}
#pragma GCC diagnostic pop
#include <vector>

namespace boson {
namespace queues {

namespace {
inline int hzdptr_size(int nprocs, int nptrs) {
  return sizeof(void * [HZDPTR_THRESHOLD(nprocs) + nptrs]);
}

inline void *_hzdptr_set(void volatile *ptr_, void *hzd_) {
  void *volatile *ptr = (void *volatile *)ptr_;
  void *volatile *hzd = (void *volatile *)hzd_;

  void *val = *ptr;
  *hzd = val;
  return val;
}

inline void *hzdptr_set(void volatile *ptr, hzdptr_t *hzd, int idx) {
  return _hzdptr_set(ptr, &hzd->ptrs[idx]);
}

inline void *_hzdptr_setv(void volatile *ptr_, void *hzd_) {
  void *volatile *ptr = (void *volatile *)ptr_;
  void *volatile *hzd = (void *volatile *)hzd_;

  void *val = *ptr;
  void *tmp;

  do {
    *hzd = val;
    tmp = val;
    FENCE();
    val = *ptr;
  } while (val != tmp);

  return val;
}

inline void *hzdptr_setv(void volatile *ptr, hzdptr_t *hzd, int idx) {
  return _hzdptr_setv(ptr, &hzd->ptrs[idx]);
}

inline void hzdptr_clear(hzdptr_t *hzd, int idx) {
  RELEASE(&hzd->ptrs[idx], NULL);
}

inline void hzdptr_retire(hzdptr_t *hzd, void *ptr) {
  void **rlist = &hzd->ptrs[hzd->nptrs];
  rlist[hzd->nretired++] = ptr;
  if (hzd->nretired == HZDPTR_THRESHOLD(hzd->nprocs)) {
    _hzdptr_retire(hzd, rlist);
  }
}

}

void lcrq::_hzdptr_enlist(hzdptr_t *hzd) {
  hzdptr_t *tail = _tail;

  if (tail == NULL) {
    hzd->next = hzd;
    if (CASra(&_tail, &tail, hzd)) return;
  }

  hzdptr_t *next = tail->next;

  do
    hzd->next = next;
  while (!CASra(&tail->next, &next, hzd));
}

int htable_insert(void **tbl, size_t size, void *ptr) {
  size_t index = XXH32(ptr, 1, 0) % size;
  size_t i;

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

int htable_lookup(void **tbl, size_t size, void *ptr) {
  size_t index = XXH32(ptr, 1, 0) % size;
  size_t i;

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

void _hzdptr_retire(hzdptr_t *hzd, void **rlist) {
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

void hzdptr_exit(hzdptr_t *hzd) {
  int i;
  void **rlist = &hzd->ptrs[hzd->nptrs];

  for (i = 0; i < hzd->nretired; ++i) {
    free(rlist[i]);
  }

  hzd->nretired = 0;
  hzd->next = hzd;
}

void *_hzdptr_setv(void volatile *ptr_, void *hzd_) {
  void *volatile *ptr = (void *volatile *)ptr_;
  void *volatile *hzd = (void *volatile *)hzd_;

  void *val = *ptr;
  void *tmp;

  do {
    *hzd = val;
    tmp = val;
    FENCE();
    val = *ptr;
  } while (val != tmp);

  return val;
}

void lcrq::lcrq_put(queue_t *q, handle_t *handle, uint64_t arg) {
  int try_close = 0;

  while (1) {
    RingQueue *rq = (RingQueue *)hzdptr_setv(&q->tail, &handle->hzdptr, 0);
    RingQueue *next = rq->next;

    if (next != NULL) {
      CAS(&q->tail, &rq, next);
      continue;
    }

    uint64_t t = FAA(&rq->tail, 1);

    if (crq_is_closed(t)) {
      RingQueue *nrq;
    alloc:
      nrq = handle->next;

      if (nrq == NULL) {
        nrq = (RingQueue *)align_malloc(PAGE_SIZE, sizeof(RingQueue));
        init_ring(nrq);
      }

      // Solo enqueue
      nrq->tail = 1;
      nrq->array[0].val = (uint64_t)arg;
      nrq->array[0].idx = 0;

      if (CAS(&rq->next, &next, nrq)) {
        CAS(&q->tail, &rq, nrq);
        handle->next = NULL;
        return;
      }
      continue;
    }

    RingNode *cell = &rq->array[t & (RING_SIZE - 1)];

    uint64_t idx = cell->idx;
    uint64_t val = cell->val;

    if (is_empty(val)) {
      if (node_index(idx) <= t) {
        if ((!node_unsafe(idx) || rq->head < static_cast<int64_t>(t)) && CAS2(cell, &val, &idx, arg, t)) {
          return;
        }
      }
    }

    uint64_t h = rq->head;

    if ((int64_t)(t - h) >= (int64_t)RING_SIZE && close_crq(rq, t, ++try_close)) {
      goto alloc;
    }
  }

  hzdptr_clear(&handle->hzdptr, 0);
}

uint64_t lcrq::lcrq_get(queue_t *q, handle_t *handle) {
  while (1) {
    RingQueue *rq = (RingQueue *)hzdptr_setv(&q->head, &handle->hzdptr, 0);
    RingQueue *next;

    uint64_t h = FAA(&rq->head, 1);

    RingNode *cell = &rq->array[h & (RING_SIZE - 1)];

    uint64_t tt = 0;
    int r = 0;

    while (1) {
      uint64_t cell_idx = cell->idx;
      uint64_t unsafe = node_unsafe(cell_idx);
      uint64_t idx = node_index(cell_idx);
      uint64_t val = cell->val;

      if (idx > h) break;

      if (!is_empty(val)) {
        if (idx == h) {
          if (CAS2(cell, &val, &cell_idx, -1, (unsafe | h) + RING_SIZE)) return val;
        } else {
          if (CAS2(cell, &val, &cell_idx, val, set_unsafe(idx))) {
            break;
          }
        }
      } else {
        if ((r & ((1ull << 10) - 1)) == 0) tt = rq->tail;

        // Optimization: try to bail quickly if queue is closed.
        int crq_closed = crq_is_closed(tt);
        uint64_t t = tail_index(tt);

        if (unsafe) {  // Nothing to do, move along
          if (CAS2(cell, &val, &cell_idx, val, (unsafe | h) + RING_SIZE)) break;
        } else if (t < h + 1 || r > 200000 || crq_closed) {
          if (CAS2(cell, &val, &idx, val, h + RING_SIZE)) {
            if (r > 200000 && tt > RING_SIZE) BTAS(&rq->tail, 63);
            break;
          }
        } else {
          ++r;
        }
      }
    }

    if (tail_index(rq->tail) <= h + 1) {
      // fixState
      while (1) {
        uint64_t t = rq->tail;
        uint64_t h = rq->head;

        if (rq->tail != static_cast<int64_t>(t)) continue;

        if (h > t) {
          if (CAS(&rq->tail, &t, h)) break;
          continue;
        }
        break;
      }
      // try to return empty
      next = rq->next;
      if (next == NULL) return -1;  // EMPTY
      if (tail_index(rq->tail) <= h + 1) {
        if (CAS(&q->head, &rq, next)) {
          hzdptr_retire(&handle->hzdptr, rq);
        }
      }
    }
  }

  hzdptr_clear(&handle->hzdptr, 0);
}

void lcrq::queue_init(queue_t *q, int nprocs) {
  RingQueue *rq = (RingQueue *)align_malloc(PAGE_SIZE, sizeof(RingQueue));
  init_ring(rq);

  q->head = rq;
  q->tail = rq;
  q->nprocs = nprocs;
}

int lcrq::close_crq(RingQueue *rq, const uint64_t t, const int tries) {
  uint64_t tt = t + 1;

  if (tries < 10)
    return CAS(&rq->tail, &tt, tt | (1ull << 63));
  else
    return BTAS(&rq->tail, 63);
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
  for (std::size_t index = 0; index < static_cast<size_t>(nprocs + 1); ++index) hds_[index] = nullptr;
}
lcrq::~lcrq() {
  //
  // Empty queue
  //
  // Create one handler to force memory reclamation algorithm
  enqueue(queue_, get_handle(nprocs_), nullptr);
  void *data = nullptr;
  while (reinterpret_cast<void *>(0xffffffffffffffff) !=
         (data = dequeue(queue_, get_handle(nprocs_))))
    ;

  //
  // Free hazard pointers
  //
  std::vector<void *> to_free(10u,nullptr);
  auto insert = [&to_free](void* ptr) {
    for (auto cur_ptr : to_free)
      if (cur_ptr == ptr) return;
    to_free.emplace_back(ptr);
  };
  
  // Clean tail
  auto tail = _tail;
  while (tail) {
    auto next_tail = tail->next;
    if (_tail == next_tail)
      break;
    for(int i=0; i < tail->nptrs; ++i)
      insert(tail->ptrs[i]);
    insert(tail->ptrs);
    tail = next_tail;
  }

  // Clean local handlers
  for (int index = 0; index < nprocs_ + 1; ++index) {
    if (hds_[index]) {
      insert(hds_[index]->hzdptr.ptrs);
      for (int i=0; i < hds_[index]->hzdptr.nptrs; ++i)
        insert(hds_[index]->hzdptr.ptrs[i]);
      insert(hds_[index]);
    }
  }

  for (void* ptr : to_free) {
    free(ptr);
  }

  //
  // Free main structs
  //
  free(queue_);
  free(hds_);
}

void lcrq::write(std::size_t proc_id, void *data) {
  assert(proc_id < static_cast<std::size_t>(nprocs_));
  enqueue(queue_, get_handle(proc_id), data);
}

void *lcrq::read(std::size_t proc_id) {
  assert(proc_id < static_cast<std::size_t>(nprocs_));
  void *result = dequeue(queue_, get_handle(proc_id));
  return reinterpret_cast<void *>(0xffffffffffffffff) == result ? nullptr : result;
}
}
}
