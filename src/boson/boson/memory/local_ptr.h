#ifndef BOSON_MEMORY_LOCAL_PTR_H_
#define BOSON_MEMORY_LOCAL_PTR_H_
#include <cassert>
#include <utility>

namespace boson {
namespace memory {

/**
 * local_ptr is like shared_ptr but is thread unsafe
 *
 * The difference with std::shared is that it does not use
 * atomics for the reference counting
 *
 * The other difference is that it can invalidated the reference whenever it wants to
 * This is secure in a thread unsafe algorithm
 */
template <class T>
class local_ptr final {
  struct references {
    std::size_t shared_refs;
    T* value;
  };

  inline void decrement() {
    if (ref_) {
      assert(0 < ref_->shared_refs);
      --ref_->shared_refs;
      if (0 == ref_->shared_refs) {
        delete ref_->value;
        delete ref_;
        ref_ = nullptr;
      }
    }
  }

  references* ref_;
 public:
  local_ptr() : ref_{nullptr} {
  }
  local_ptr(T* in_value) : ref_{new references{1u, in_value}} {
  }

  local_ptr(T&& in_value) : ref_{new references{1u, new T{std::move(in_value)}}} {
  }

  local_ptr(local_ptr const& other) : ref_{other.ref_} {
    if (ref_)
      ++ref_->shared_refs;
  }

  local_ptr(local_ptr && other) : ref_{other.ref_} {
    other.ref_ = nullptr;
  }

  local_ptr& operator=(local_ptr const& other)  {
    decrement();
    ref_ = other.ref_;
    if (ref_)
      ++ref_->shared_refs;
    return *this;
  }

  local_ptr& operator=(local_ptr && other)  {
    decrement();
    ref_ = other.ref_;
    other.ref_ = nullptr;
    return *this;
  }

  local_ptr& operator=(std::nullptr_t)  {
    decrement();
    ref_ = nullptr;
    return *this;
  }

  ~local_ptr()  {
    decrement();
  }

  inline void reset(T* new_value = nullptr) {
    // Assert ref ?
    if (ref_) {
      delete ref_->value;
      ref_->value = new_value;
    }
  }

  inline void invalidate_all() {
    reset();
  }

  inline operator bool () const {
    return ref_ && ref_->value;
  }

  T& operator*() const {
    assert(ref_ && ref_->value);
    return *ref_->value;
  }

  T* operator->() const {
    assert(ref_ && ref_->value);
    return ref_->value;
  }
};

}  // namespace memory
}  // namespace boson

#endif // BOSON_MEMORY_LOCAL_PTR_H_
