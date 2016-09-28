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

namespace boson {
namespace queues {

static constexpr std::size_t PAGE_SIZE = 4096;
static constexpr std::size_t CACHE_LINE_SIZE = 64;
static constexpr std::size_t MAX_SPIN = 100;
static constexpr int MAX_PATIENCE = 10;
template <class T> constexpr T MAX_GARBAGE(T n) { return 2*n; }
// static constexpr void * BOT = 0;
// static constexpr void * TOP = reinterpret_cast<void*>(-1);
#define BOT ((void*)0)
#define TOP ((void*)-1)

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
  static constexpr std::memory_order const order_seq_cst= std::memory_order::memory_order_seq_cst;

  struct alignas(CACHE_LINE_SIZE) enq_t {
    std::atomic<long> id;
    std::atomic<void*> val;
  };

  struct alignas(CACHE_LINE_SIZE) deq_t {
    std::atomic<long> id;
    std::atomic<long> idx;
  };

  struct cell_t {
    std::atomic<void*> val;
    std::atomic<enq_t*> enq;
    std::atomic<deq_t*> deq;
    void* pad[5];
  };

  struct node_t {
    alignas(CACHE_LINE_SIZE) std::atomic<node_t*> next;
    alignas(CACHE_LINE_SIZE) long id;
    alignas(CACHE_LINE_SIZE) cell_t cells[WFQUEUE_NODE_SIZE];
  };

  struct handle_t {
    std::atomic<handle_t*> next;
    std::atomic<node_t*> Hp;
    std::atomic<node_t*> Ep;
    std::atomic<node_t*> Dp;
    alignas(CACHE_LINE_SIZE) enq_t Er;
    alignas(CACHE_LINE_SIZE) deq_t Dr;
    alignas(CACHE_LINE_SIZE) handle_t* Eh;
    long Ei;
    handle_t* Dh;
    alignas(CACHE_LINE_SIZE) node_t* spare;
    int delay;
  };

  alignas(2 * CACHE_LINE_SIZE) std::atomic<long> Ei;
  alignas(2 * CACHE_LINE_SIZE) std::atomic<long> Di;
  alignas(2 * CACHE_LINE_SIZE) std::atomic<long> Hi;
  std::atomic<node_t*> Hp;
  long nprocs;
  handle_t* hds_;

  void enqueue(handle_t* th, void* v) {
    th->Hp = th->Ep.load(order_relaxed);

    long id;
    int p = MAX_PATIENCE;
    while (!this->enq_fast(th, v, &id) && p-- > 0)
      ;
    if (p < 0) this->enq_slow(th, v, id);
    th->Hp.store(nullptr, std::memory_order_release);
  }

  inline node_t* new_node() {
    void* n = nullptr;
    int ret = ::posix_memalign(&n, PAGE_SIZE, sizeof(node_t));
    assert(ret == 0);
    std::memset(n, 0, sizeof(node_t));
    return static_cast<node_t*>(n);
  }

  cell_t* find_cell(std::atomic<node_t*>* ptr, long i, handle_t* th) {
    node_t* curr = ptr->load(order_relaxed);
    long j;
    for (j = curr->id; j < i / WFQUEUE_NODE_SIZE; ++j) {
      node_t* next = curr->next.load(order_relaxed);
      if (next == nullptr) {
        node_t* temp = th->spare;
        if (!temp) {
          temp = new_node();
          th->spare = temp;
        }
        temp->id = j + 1;
        if (curr->next.compare_exchange_strong(next, temp, order_release, order_acquire)) {
          next = temp;
          th->spare = nullptr;
        }
      }
      curr = next;
    }

    ptr->store(curr, std::memory_order_relaxed);  // Hum
    return &curr->cells[i % WFQUEUE_NODE_SIZE];
  }

  int enq_fast(handle_t* th, void* v, long* id) {
    long i = this->Ei.fetch_add(1, std::memory_order_seq_cst);
    cell_t* c = find_cell(&th->Ep, i, th);
    void* cv = BOT;

    if (c->val.compare_exchange_strong(cv, v, order_relaxed, order_relaxed)) {
      return 1;
    } else {
      *id = i;
      return 0;
    }
  }
  void enq_slow(handle_t* th, void* v, long id) {
    enq_t* enq = &th->Er;
    enq->val.store(v, order_relaxed);  // hum
    enq->id.store(id, order_release);

    std::atomic<node_t*> tail {th->Ep.load(order_relaxed)};
    long i = 0;
    cell_t* c = nullptr;

    do {
      i = this->Ei.fetch_add(1, order_relaxed);
      c = find_cell(&tail, i, th);
      enq_t* ce = static_cast<enq_t*>(BOT);

      if (c->enq.compare_exchange_strong(ce, enq) &&
          c->val.load(order_relaxed) != TOP) {
        if (enq->id.compare_exchange_strong(id, -i, order_relaxed, order_relaxed))
          id = -i;
        break;
      }
    } while (enq->id.load(order_relaxed) > 0);

    id = -enq->id;
    c = find_cell(&th->Ep, id, th);
    if (id > i) {
      long Ei = this->Ei.load(order_relaxed);  // hum
      while (Ei <= id &&
             !this->Ei.compare_exchange_strong(Ei, id + 1, order_relaxed, order_relaxed))
        ;
    }
    c->val.store(v, std::memory_order_relaxed);
  }

  void queue_register(handle_t * th)
  {
    th->next.store(nullptr);
    th->Hp.store(nullptr);
    th->Ep.store(Hp);
    th->Dp.store(Hp);

    th->Er.id = 0;
    th->Er.val = BOT;
    th->Dr.id = 0;
    th->Dr.idx = -1;

    th->Ei = 0;
    th->spare = new_node();

    static std::atomic<handle_t*> _tail;
    handle_t * tail = _tail;

    if (tail == nullptr) {
      th->next = th;
      if (_tail.compare_exchange_strong(tail, th, order_release, order_acquire)) {
        th->Eh = th->next;
        th->Dh = th->next;
        return;
      }
    }

    handle_t * next {tail->next};
    do th->next.store(next);
    while (!tail->next.compare_exchange_strong(next, th, order_release, order_acquire));

    th->Eh = th->next;
    th->Dh = th->next;
  }

  void cleanup(handle_t* th) {
    long oid = Hi;
    node_t* new_node = th->Dp;

    if (oid == -1) return;
    if (new_node->id - oid < MAX_GARBAGE(nprocs)) return;
    if (!Hi.compare_exchange_strong(oid, -1, order_acquire, order_relaxed)) return;

    node_t* old = Hp;
    handle_t* ph = th;
    handle_t* phs[nprocs];
    int i = 0;

    do {
      node_t* Hp = ph->Hp.load(order_acquire);
      if (Hp && Hp->id < new_node->id) new_node = Hp;

      new_node = update(&ph->Ep, new_node, &ph->Hp);
      new_node = update(&ph->Dp, new_node, &ph->Hp);

      phs[i++] = ph;
      ph = ph->next;
    } while (new_node->id > oid && ph != th);

    while (new_node->id > oid && --i >= 0) {
      node_t* Hp = phs[i]->Hp.load(order_acquire);
      if (Hp && Hp->id < new_node->id) new_node = Hp;
    }

    long nid = new_node->id;

    if (nid <= oid) {
      Hi.store(oid, order_release);
    } else {
      Hp = new_node;
      Hi.store(nid, order_release);

      while (old != new_node) {
        node_t* tmp = old->next;
        free(old);
        old = tmp;
      }
    }
  }

  node_t* update(std::atomic<node_t*>* pPn, node_t* cur, std::atomic<node_t*>* pHp) {
    node_t* ptr = pPn->load(order_relaxed);

    if (ptr->id < cur->id) {
      if (!pPn->compare_exchange_strong(ptr, cur, order_seq_cst, order_seq_cst)) {
        if (ptr->id < cur->id) cur = ptr;
      }

      node_t* Hp = *pHp;
      if (Hp && Hp->id < cur->id) cur = Hp;
    }

    return cur;
  }

 public:
  using content_type = ContentType;

  class queue_handler {
    wfqueue& parent_;
    handle_t* handler_;

  };

  wfqueue(int i_nprocs) noexcept(std::is_nothrow_default_constructible<ContentType>()) {
    Hi = 0;
    Hp = new_node();
    Ei = 1;
    Di = 1;
    nprocs = i_nprocs;
    //pthread_barrier_init(&barrier, NULL, nprocs);
    void* hds_addr_ = static_cast<void*>(hds_);
    int ret = ::posix_memalign(&hds_addr_, PAGE_SIZE, sizeof(handle_t * [nprocs]));
    assert(ret == 0);
    hds_ = static_cast<handle_t*>(hds_addr_);
    for (std::size_t index = 0; index < nprocs; ++index)
      queue_register(hds_ + index);
  }

  wfqueue(wfqueue const&) = delete;
  wfqueue(wfqueue&&) = default;
  wfqueue& operator=(wfqueue const&) = delete;
  wfqueue& operator=(wfqueue&&) = default;
  ~wfqueue() {
    for (std::size_t index = 0; index < nprocs; ++index)
      cleanup(hds_ + index);
    free(hds_);
  }
  
  template <class ... Args>
  void push(std::size_t proc_id, Args&& ... args) {
    enqueue(hds_ + proc_id, static_cast<void*>(new ContentType(std::forward<Args>(args)...)));
  }
};

};  // namespace queues
};  // namespace boson

#endif  // BOSON_QUEUES_WFQUEUE_H_
