#ifndef BOSON_MUTEX_H_
#define BOSON_MUTEX_H_
#include "semaphore.h"

namespace boson {

class mutex : private semaphore {
 public:
  inline mutex();
  mutex(mutex const&) = delete;
  mutex(mutex&&) = default;
  mutex& operator=(mutex const&) = delete;
  mutex& operator=(mutex&&) = default;
  virtual ~mutex() = default;
  inline void lock();
  inline void unlock();
};

mutex::mutex() : semaphore(1) {
}

void mutex::lock() {
  wait();
}
void mutex::unlock() {
  post();
}

/**
 * shared_mutex is a wrapper for shared_ptr of a mutex
 */
class shared_mutex {
  std::shared_ptr<mutex> impl_;

 public:
  inline shared_mutex();
  shared_mutex(shared_mutex const&) = default;
  shared_mutex(shared_mutex&&) = default;
  shared_mutex& operator=(shared_mutex const&) = default;
  shared_mutex& operator=(shared_mutex&&) = default;
  virtual ~shared_mutex() = default;

  inline void lock();
  inline void unlock();
};

// inline implementations

shared_mutex::shared_mutex() : impl_{new mutex} {
}

void shared_mutex::lock() {
  impl_->lock();
}

void shared_mutex::unlock() {
  impl_->unlock();
}

}  // namespace boson

#endif  // BOSON_MUTEX_H_
