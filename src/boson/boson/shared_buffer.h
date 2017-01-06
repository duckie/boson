#ifndef BOSON_SHARED_BUFFER_H_
#define BOSON_SHARED_BUFFER_H_
#pragma once

#include "internal/routine.h"
#include "internal/thread.h"
#include "logger.h"
#include <cassert>

namespace boson {
template <class Value>

/**
 * A thread local POD buffer for IO
 *
 * This class is useful when doing similar IO calls such as reads
 * from the same routine instantiated multiple times. Instead of maintaining
 * an allocated buffer for each routine, using this object allows you
 * to use a buffer shared with other routines. The only data valid in the 
 * buffer is data the routine itself wrote in it, and is valid until the next
 * context switch.
 *
 * That means that:
 * - Any boson system call made after the buffer modification invalidates
 *   the buffer (even a simple yield or sleep).
 * - You cannot use the buffer to share data between routines, as it may be
 *   overwritten anytime (between context switche) by any other buffer of 
 *   the same size
 */
class shared_buffer {
  static_assert(std::is_pod<Value>::value, "value_type of a shared buffer must be a POD.");

  Value& buffer_;

 public:
  using value_type = Value;
  shared_buffer()
      : buffer_{*reinterpret_cast<Value*>(internal::current_thread()->get_shared_buffer(sizeof(Value)))} {
  }

  inline Value& get() { return buffer_; }
  inline Value const& get() const { return buffer_; }
};

}

#endif  // BOSON_SHARED_BUFFER_H_
