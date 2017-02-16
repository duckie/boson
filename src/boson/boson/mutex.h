#ifndef BOSON_MUTEX_H_
#define BOSON_MUTEX_H_
#include "semaphore.h"

namespace boson {

/**
 * mutex is a wrapper for shared_ptr of a mutex_impl
 */
class mutex {
  friend class internal::select_impl::event_semaphore_wait_base_storage;
  template <class>
  friend class internal::select_impl::event_mutex_lock_storage;
  shared_semaphore impl_;

 public:
  inline mutex();
  mutex(mutex const&) = default;
  mutex(mutex&&) = default;
  mutex& operator=(mutex const&) = default;
  mutex& operator=(mutex&&) = default;
  virtual ~mutex() = default;

  inline void lock(int timeout = -1);
  inline void lock(std::chrono::milliseconds timeout);
  inline void unlock();
};

// inline implementations

mutex::mutex() : impl_{1} {
}

void mutex::lock(int timeout) {
  impl_.wait(timeout);
}

void mutex::lock(std::chrono::milliseconds timeout) {
  impl_.wait(timeout);
}

void mutex::unlock() {
  impl_.post();
}

}  // namespace boson

#endif  // BOSON_MUTEX_H_
