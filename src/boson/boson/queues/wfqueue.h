#ifndef BOSON_QUEUES_WFQUEUE_H_
#define BOSON_QUEUES_WFQUEUE_H_
#include <atomic>
#include <cstdint>
#include <type_traits>
#include <cassert>
#include <unistd.h>
#include <cstdlib>
#include <cstring>

namespace boson {
namespace queues {

static constexpr std::size_t PAGE_SIZE = 4096;
static constexpr std::size_t CACHE_LINE_SIZE = 64;
static constexpr std::size_t MAX_SPIN = 100;
static constexpr int MAX_PATIENCE = 10;
//static constexpr void * BOT = 0;
//static constexpr void * TOP = reinterpret_cast<void*>(-1);
#define BOT ((void *) 0)
#define TOP ((void *)-1)

/**
 * Wfqueue is a wait-free MPMC concurrent queue
 *
 * @see https://github.com/chaoran/fast-wait-free-queue
 *
 * Algorithm from Chaoran Yang and John Mellor-Crummey
 */
template <class ContentType>
class alignas(2 * CACHE_LINE_SIZE) wfqueue {
  //#define CACHE_ALIGNED __attribute__((aligned(CACHE_LINE_SIZE)))
  //#define DOUBLE_CACHE_ALIGNED __attribute__((aligned(2 * CACHE_LINE_SIZE)))
  using content_t = ContentType;
  static constexpr std::size_t WFQUEUE_NODE_SIZE = ((1 << 10) - 2);
  static constexpr std::memory_order const order_relaxed = std::memory_order::memory_order_relaxed;
  static constexpr std::memory_order const order_acquire = std::memory_order::memory_order_acquire;
  static constexpr std::memory_order const order_release = std::memory_order::memory_order_release;

  struct alignas(CACHE_LINE_SIZE) enq_t {
    std::atomic<long> volatile id;
    std::atomic<void*> volatile val;
  };

  struct alignas(CACHE_LINE_SIZE) deq_t {
    std::atomic<long> volatile id;
    std::atomic<long> volatile idx;
  };

  struct cell_t {
    std::atomic<ContentType> volatile val;
    std::atomic<enq_t*> volatile enq;
    std::atomic<deq_t*> volatile deq;
    void* pad[5];
  };

  struct node_t {
    alignas(CACHE_LINE_SIZE) std::atomic<node_t*> volatile next;
    alignas(CACHE_LINE_SIZE) long id;
    alignas(CACHE_LINE_SIZE) cell_t cells[WFQUEUE_NODE_SIZE];
  };

  struct handle_t {
    handle_t * next;
    std::atomic<node_t*> volatile Hp;
    std::atomic<node_t*> volatile Ep;
    std::atomic<node_t*> volatile Dp;
    alignas(CACHE_LINE_SIZE) enq_t Er;
    alignas(CACHE_LINE_SIZE) deq_t Dr;
    alignas(CACHE_LINE_SIZE) handle_t * Eh;
    long Ei;
    handle_t * Dh;
    alignas(CACHE_LINE_SIZE) node_t * spare;
    int delay;
  };

  alignas(2 * CACHE_LINE_SIZE) volatile std::atomic<long> Ei;
  alignas(2 * CACHE_LINE_SIZE) volatile std::atomic<long> Di;
  alignas(2 * CACHE_LINE_SIZE) volatile std::atomic<long> Hi;
  std::atomic<node_t*> volatile Hp;
  long nprocs;

  void enqueue(handle_t * th, void * v)
  {
    th->Hp = th->Ep.load(std::memory_order::memory_order_relaxed);

    long id;
    int p = MAX_PATIENCE;
    while (!this->enq_fast(th, v, &id) && p-- > 0);
    if (p < 0) 
      this->enq_slow(th, v, id);
    th->Hp.store(nullptr,std::memory_order_release);
  }

  inline node_t* new_node() {
    node_t* n = nullptr;
    int ret = ::posix_memalign(&n, PAGE_SIZE, sizeof(node_t));
    assert(ret == 0);
    std::memset(n, 0, sizeof(node_t));
    return n;
  }

  cell_t* find_cell(std::atomic<node_t*>* ptr, long i, handle_t* th) {
    node_t* curr = ptr->load(std::memory_order::memory_order_relaxed);
    long j;
    for (j = curr->id; j < i / WFQUEUE_NODE_SIZE; ++j) {
      node_t* next = curr->next->load(std::memory_order::memory_order_relaxed);
      if (next == nullptr) {
        node_t* temp = th->spare;
        if (!temp) {
          temp = new_node();
          th->spare = temp;
        }
        temp->id = j + 1;
        if (curr->next.compare_exchange_strong(&next, temp, std::memory_order::memory_order_release,
                                               std::memory_order::memory_order_acquire)) {
          next = temp;
          th->spare = nullptr;
        }
      }
      curr = next;
    }

    ptr->store(curr, std::memory_order_relaxed);  // Hum
    return &curr->cells[i % WFQUEUE_NODE_SIZE];
  }

  int enq_fast(handle_t * th, void * v, long * id)
  {
    long i = this->Ei.fetch_add(1,std::memory_order_seq_cst);
    cell_t * c = find_cell(&th->Ep, i, th);
    void * cv = BOT;

    if (c->val.compare_exchange_strong(cv, v, std::memory_order::memory_order_relaxed,
                                       std::memory_order::memory_order_relaxed)) {
      return 1;
    } else {
      *id = i;
      return 0;
    }
  }
  void enq_slow(handle_t* th, void* v, long id) {
    enq_t* enq = th->Er.load(std::memory_order::memory_order_relaxed);
    enq->val.store(v,std::memory_order::memory_order_relaxed);  // hum
    enq->id.store(id,std::memory_order::memory_order_release);

    node_t* tail = th->Ep.load(std::memory_order::memory_order_relaxed);
    long i = 0;
    cell_t* c = nullptr;

    do {
      i = this->Ei.fetch_add(1,std::memory_order::memory_order_relaxed);
      c = find_cell(&tail, i, th);
      enq_t* ce = BOT;

      if (c->enq.compare_exchange_strong(ce, enq) && c->val.load(std::memory_order::memory_order_relaxed) != TOP) {
        if (enq->id.compare_exchange_strong(id, -i, std::memory_order::memory_order_relaxed,
                                            std::memory_order::memory_order_relaxed))
          id = -i;
        break;
      }
    } while (enq->id.load(std::memory_order::memory_order_relaxed) > 0);

    id = -enq->id;
    c = find_cell(&th->Ep, id, th);
    if (id > i) {
      long Ei = this->Ei.load(std::memory_order::memory_order_relaxed); // hum
      while (Ei <= id &&
             !this->Ei.compare_exchange_strong(&Ei, id + 1, std::memory_order::memory_order_relaxed,
                                               std::memory_order::memory_order_relaxed))
        ;
    }
    c->val.store(v,std::memory_order_relaxed);
  }

 public:
  using content_type = ContentType;

  class queue_handler {
    wfqueue& parent_;
    handle_t

  };

  wfqueue() noexcept(std::is_nothrow_default_constructible<ContentType>()) = default;
  wfqueue(wfqueue const&) = delete;
  wfqueue(wfqueue&&) noexcept = default;
  wfqueue& operator=(wfqueue const&) = delete;
  wfqueue& operator=(wfqueue&&) noexcept = default;
  ~wfqueue() = default;

};

};  // namespace queues
};  // namespace boson

#endif  // BOSON_QUEUES_WFQUEUE_H_
