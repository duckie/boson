#ifndef BOSON_QUEUES_LCRQ_H_
#define BOSON_QUEUES_LCRQ_H_
#include <unistd.h>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <list>
#include <type_traits>
#include <utility>
#ifdef BOSON_DISABLE_LOCKFREE
#include <mutex>
#endif

namespace boson {
namespace queues {

#ifndef BOSON_DISABLE_LOCKFREE

constexpr int HZDPTR_THRESHOLD(int nprocs) {
return 2 * nprocs;
}
constexpr int HZDPTR_HTBL_SIZE(int nprocs, int nptrs) {
return 4 * nprocs * nptrs;
}

typedef struct _hzdptr_t {
  struct _hzdptr_t *next;
  int nprocs;
  int nptrs;
  int nretired;
  void **ptrs;
} hzdptr_t;

int htable_insert(void **tbl, size_t size, void *ptr);
int htable_lookup(void **tbl, size_t size, void *ptr);
void _hzdptr_retire(hzdptr_t *hzd, void **rlist);
void hzdptr_exit(hzdptr_t *hzd);

class lcrq {

  static constexpr size_t RING_SIZE = (1ull << 12);

  typedef struct _node_t { struct _node_t *next; } node_t;

  hzdptr_t *volatile _tail = nullptr;

  void hzdptr_init(hzdptr_t *hzd, int nprocs, int nptrs);
  void _hzdptr_enlist(hzdptr_t *hzd);

  typedef struct alignas(128) RingNode {
    volatile uint64_t val;
    volatile uint64_t idx;
    uint64_t pad[14];
  } RingNode;

  typedef struct alignas(128) RingQueue {
    alignas(128) volatile int64_t head;
    alignas(128) volatile int64_t tail;
    alignas(128) struct RingQueue *next;
    RingNode array[RING_SIZE];
  } RingQueue;

  typedef struct {
    alignas(128) RingQueue *volatile head;
    alignas(128) RingQueue *volatile tail;
    int nprocs;
  } queue_t;

  typedef struct {
    RingQueue *next;
    hzdptr_t hzdptr;
  } handle_t;

  inline void init_ring(RingQueue *r) {
    size_t i;
    for (i = 0; i < RING_SIZE; i++) {
      r->array[i].val = -1;
      r->array[i].idx = i;
    }
    r->head = r->tail = 0;
    r->next = NULL;
  }

  inline int is_empty(uint64_t v) __attribute__((pure)) {
    return (v == (uint64_t)-1);
  }

  inline uint64_t node_index(uint64_t i) __attribute__((pure)) {
    return (i & ~(1ull << 63));
  }

  inline uint64_t set_unsafe(uint64_t i) __attribute__((pure)) {
    return (i | (1ull << 63));
  }

  inline uint64_t node_unsafe(uint64_t i) __attribute__((pure)) {
    return (i & (1ull << 63));
  }

  inline uint64_t tail_index(uint64_t t) __attribute__((pure)) {
    return (t & ~(1ull << 63));
  }

  inline int crq_is_closed(uint64_t t) __attribute__((pure)) {
    return (t & (1ull << 63)) != 0;
  }

  void queue_init(queue_t *q, int nprocs);
  int close_crq(RingQueue *rq, const uint64_t t, const int tries);
  void lcrq_put(queue_t *q, handle_t *handle, uint64_t arg);
  uint64_t lcrq_get(queue_t *q, handle_t *handle);

  inline void queue_register(queue_t *q, handle_t *th, int id) {
    hzdptr_init(&th->hzdptr, q->nprocs, 1);
  }

  inline void enqueue(queue_t *q, handle_t *th, void *val) {
    lcrq_put(q, th, (uint64_t)val);
  }

  inline void *dequeue(queue_t *q, handle_t *th) {
    return (void *)lcrq_get(q, th);
  }

  queue_t *queue_;
  handle_t **hds_;
  int nprocs_;
  handle_t *get_handle(std::size_t proc_id);

 public:
  lcrq(int nprocs);
  lcrq(lcrq const &) = delete;
  lcrq(lcrq &&) = default;
  lcrq &operator=(lcrq const &) = delete;
  lcrq &operator=(lcrq &&) = default;
  ~lcrq();
  void write(std::size_t proc_id, void *data);
  void *read(std::size_t proc_id);
};

#else  // ifndef BOSON_DISABLE_LOCKFREE

class lcrq {
  std::mutex lock_;
  std::list<void*> queue_;

 public:
  lcrq(int nprocs);
  lcrq(lcrq const &) = delete;
  lcrq(lcrq &&) = default;
  lcrq &operator=(lcrq const &) = delete;
  lcrq &operator=(lcrq &&) = default;
  ~lcrq() = default;
  void write(std::size_t proc_id, void *data);
  void *read(std::size_t proc_id);
};

#endif  // ifndef BOSON_DISABLE_LOCKFREE else

};  // namespace queues
};  // namespace boson

#endif  // BOSON_QUEUES_LCRQ_H_
