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

}  // namespace boson

#endif  // BOSON_MUTEX_H_
