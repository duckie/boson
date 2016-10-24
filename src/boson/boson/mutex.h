#ifndef BOSON_MUTEX_H_
#define BOSON_MUTEX_H_
#include "semaphore.h"

namespace boson {

class mutex_impl : private semaphore {
 public:
  inline mutex_impl();
  mutex_impl(mutex_impl const&) = delete;
  mutex_impl(mutex_impl&&) = default;
  mutex_impl& operator=(mutex_impl const&) = delete;
  mutex_impl& operator=(mutex_impl&&) = default;
  virtual ~mutex_impl() = default;
  inline void lock(int timeout = -1);
  inline void lock(std::chrono::milliseconds timeout = std::chrono::milliseconds(0));
  inline void unlock();
};

mutex_impl::mutex_impl() : semaphore(1) {
}

void mutex_impl::lock(int timeout) {
  wait(timeout);
}

void mutex_impl::lock(std::chrono::milliseconds timeout) {
  wait(timeout);
}

void mutex_impl::unlock() {
  post();
}

/**
 * mutex is a wrapper for shared_ptr of a mutex_impl
 */
class mutex {
  std::shared_ptr<mutex_impl> impl_;

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

mutex::mutex() : impl_{new mutex_impl} {
}

void mutex::lock(int timeout) {
  impl_->lock(timeout);
}

void mutex::lock(std::chrono::milliseconds timeout) {
  impl_->lock(timeout);
}

void mutex::unlock() {
  impl_->unlock();
}

}  // namespace boson

#endif  // BOSON_MUTEX_H_
