#ifndef BOSON_QUEUES_WFQUEUE_H_
#define BOSON_QUEUES_WFQUEUE_H_
#include <atomic>
#include <cstdint>
#include <type_traits>

namespace boson {
namespace queues {

static constexpr size_t PAGE_SIZE = 4096;
static constexpr size_t CACHE_LINE_SIZE = 64;

/**
 * Wfqueue is a wait-free MPMC concurrent queue
 *
 * @see https://github.com/chaoran/fast-wait-free-queue
 *
 * Algorithm from Chaoran Yang and John Mellor-Crummey
 */
template <class ContentType>
class alignas(2*CACHE_LINE_SIZE) wfqueue {
  //#define CACHE_ALIGNED __attribute__((aligned(CACHE_LINE_SIZE)))
  //#define DOUBLE_CACHE_ALIGNED __attribute__((aligned(2 * CACHE_LINE_SIZE)))
  using content_t = ContentType;
  static constexpr size_t WFQUEUE_NODE_SIZE = ((1 << 10) - 2);
  static constexpr std::memory_order const order_relaxed = std::memory_order::memory_order_relaxed;
  static constexpr std::memory_order const order_acquire = std::memory_order::memory_order_acquire;
  static constexpr std::memory_order const order_release = std::memory_order::memory_order_release;

  struct enq_t alignas(CACHE_LINE_SIZE) {
    long volatile id;
    void* volatile val;
  };

  struct deq_t alignas(CACHE_LINE_SIZE) {
    long volatile id;
    long volatile idx;
  };

  struct cell_t {
    void* volatile val;
    struct enq_t* volatile enq;
    struct deq_t* volatile deq;
    void* pad[5];
  };

  struct node_t {
    alignas(CACHE_LINE_SIZE) node_t* volatile next;
    alignas(CACHE_LINE_SIZE) long id;
    alignas(CACHE_LINE_SIZE) cell_t cells[WFQUEUE_NODE_SIZE];
  };

  alignas(2*CACHE_LINE_SIZE) volatile long Ei;
  alignas(2*CACHE_LINE_SIZE) volatile long Di;
  alignas(2*CACHE_LINE_SIZE) volatile long Hi;
  struct _node_t * volatile Hp;
  long nprocs;

 public:
  using content_type = ContentType;
  static constexpr std::size_t const size = Size;

  wfqueue() noexcept(std::is_nothrow_default_constructible<ContentType>()) = default;
  wfqueue(wfqueue const&) = delete;
  wfqueue(wfqueue&&) noexcept(false) = default;
  wfqueue& operator=(wfqueue const&) = delete;
  wfqueue& operator=(wfqueue&&) noexcept(false) = default;
  ~wfqueue() = default;

};

};  // namespace queues
};  // namespace boson

#endif  // BOSON_QUEUES_WFQUEUE_H_
