#ifndef BOSON_SYSCALL_TRAITS_H_
#define BOSON_SYSCALL_TRAITS_H_

#include <sys/socket.h>
#include <chrono>
#include <cstdint>
#include <utility>
#include <sys/syscall.h>
#include <sys/types.h>
#include "internal/routine.h"
#include "system.h"
#include "std/experimental/apply.h"
#include "std/experimental/chrono.h"
#include <utility>

namespace boson {
/**
 * Bind a syscall to a type
 *
 * This structs represents a kernel syscall within a type
 */
template <int SyscallId> struct syscall_callable {
  template <class ... Args> static inline decltype(auto) call(Args&& ... args) {
    return ::syscall(SyscallId, std::forward<Args>(args)...);
  }

  template <class TupleArgs> static inline decltype(auto) apply_call(TupleArgs&& tuple_args) {
    return experimental::apply(::syscall, std::tuple_cat(std::make_tuple(SyscallId), tuple_args));
  }
};

/**
 * Applies the right event add depending on the syscall
 */
template <bool IsRead>
struct add_event;
template <>
struct add_event<true> {
  static inline void apply(internal::routine* current, int fd) {
    current->add_read(fd);
  }
};
template <>
struct add_event<false> {
  static inline void apply(internal::routine* current, int fd) {
    current->add_write(fd);
  }
};

template <bool HasTimer>
struct add_timer;
template <>
struct add_timer<true> {
  static inline void apply(internal::routine* current, int timeout_ms) {
    current->add_timer(experimental::chrono::ceil<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(timeout_ms)));
  }
};
template <> struct add_timer<false> {
  static inline void apply(internal::routine*, int) {
    // Do nothin
  }
};

template <int SyscallId> struct syscall_traits;

template <> struct syscall_traits<SYS_read> {
  static constexpr bool is_read = true;
};

template <> struct syscall_traits<SYS_write> {
  static constexpr bool is_read = false;
};

template <> struct syscall_traits<SYS_recvfrom> {
  static constexpr bool is_read = true;
};

template <> struct syscall_traits<SYS_sendto> {
  static constexpr bool is_read = false;
};

template <> struct syscall_traits<SYS_accept> {
  static constexpr bool is_read = true;
};

template <> struct syscall_traits<SYS_connect> {
  static constexpr bool is_read = false;
};

}  // namespace boson

#endif  // BOSON_SYSCALL_TRAITS_H_
