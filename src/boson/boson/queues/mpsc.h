// This is free and unencumbered software released into the public domain.

// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.

// In jurisdictions that recognize copyright laws, the author or authors
// of this software dedicate any and all copyright interest in the
// software to the public domain. We make this dedication for the benefit
// of the public at large and to the detriment of our heirs and
// successors. We intend this dedication to be an overt act of
// relinquishment in perpetuity of all present and future rights to this
// software under copyright law.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

// For more information, please refer to <http://unlicense.org/>

// C++ implementation of Dmitry Vyukov's non-intrusive lock free unbound MPSC queue
// http://www.1024cores.net/home/lock-free-algorithms/queues/non-intrusive-mpsc-node-based-queue

#ifndef BOSON_QUEUES_MPSC_H_
#define BOSON_QUEUES_MPSC_H_

#include <atomic>
#include <cstring>
#include <type_traits>
#include <utility>
#include "../utility.h"

namespace boson {
namespace queues {

template <typename T>
class mpsc {
  template <class U, std::enable_if_t<!is_unique_ptr<U>{}, int> = 0>
  inline static void del(U& data) {
    data.~U();
  }

  template <class U, std::enable_if_t<is_unique_ptr<U>{}, int> = 0>
  inline static void del(U&) {
  }

 public:
  mpsc()
      : _head(reinterpret_cast<buffer_node_t*>(new buffer_node_aligned_t)),
        _tail(_head.load(std::memory_order_relaxed)) {
    buffer_node_t* front = _head.load(std::memory_order_relaxed);
    ::memset(&front->data,0,sizeof(buffer_node_aligned_t));
    front->next.store(nullptr, std::memory_order_relaxed);
  }

  ~mpsc() {
    T output;
    while (this->read(output)) {
    }
    buffer_node_t* front = _head.load(std::memory_order_relaxed);
    delete front;
  }

  void write(T input) {
    buffer_node_t* node =
        reinterpret_cast<buffer_node_t*>(new buffer_node_aligned_t);
    new (&node->data ) T(std::move(input));
    node->next.store(nullptr,std::memory_order_relaxed);
    buffer_node_t* prev_head = _head.exchange(node, std::memory_order_acq_rel);
    prev_head->next.store(node, std::memory_order_release);
  }

  bool read(T& output) {
    buffer_node_t* tail = _tail.load(std::memory_order_relaxed);
    buffer_node_t* next = tail->next.load(std::memory_order_acquire);
    if (next == nullptr) {
      return false;
    }
    output = std::move(next->data);
    del(next->data);
    _tail.store(next, std::memory_order_release);
    delete tail;
    return true;
  }

 private:
  struct buffer_node_t {
    T data;
    std::atomic<buffer_node_t*> next;
  };

  typedef typename std::aligned_storage<
      sizeof(buffer_node_t), std::alignment_of<buffer_node_t>::value>::type buffer_node_aligned_t;

  std::atomic<buffer_node_t*> _head;
  std::atomic<buffer_node_t*> _tail;
};
}
}

#endif
