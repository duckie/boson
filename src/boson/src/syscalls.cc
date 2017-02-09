#include "boson/syscalls.h"
#include "boson/internal/routine.h"
#include "boson/internal/thread.h"
#include <sys/syscall.h>
#include <sys/types.h>

namespace boson {

using namespace internal;

namespace {

/**
 * Bind a syscall to a type
 *
 * This structs represents a kernel syscall within a type
 */
template <int SyscallId> struct syscall_callable {
  template <class ... Args> static inline decltype(auto) call(Args&& ... args) {
    return ::syscall(SyscallId, std::forward<Args>(args)...);
  }
};

template <bool IsRead> struct add_event;
template <> struct add_event<true> {
  static inline void apply(routine* current, int fd) {
    current->add_read(fd);
  }
};
template <> struct add_event<false> {
  static inline void apply(routine* current, int fd) {
    current->add_write(fd);
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
}

void yield() {
  thread* this_thread = current_thread();
  routine* current_routine = this_thread->running_routine();
  current_routine->status_ = routine_status::yielding;
  this_thread->context() = jump_fcontext(this_thread->context().fctx, nullptr);
  current_routine->previous_status_ = routine_status::yielding;
  current_routine->status_ = routine_status::running;
}

void sleep(std::chrono::milliseconds duration) {
  // Compute the time in ms
  using namespace std::chrono;
  thread* this_thread = current_thread();
  routine* current_routine = this_thread->running_routine();
  current_routine->start_event_round();
  current_routine->add_timer(
      time_point_cast<milliseconds>(high_resolution_clock::now() + duration));
  current_routine->commit_event_round();
  current_routine->previous_status_ = routine_status::wait_events;
  current_routine->status_ = routine_status::running;
}

template <bool IsARead>
int wait_readiness(fd_t fd, int timeout_ms) {
  using namespace std::chrono;
  thread* this_thread = current_thread();
  routine* current_routine = this_thread->running_routine();
  current_routine->start_event_round();
  add_event<IsARead>::apply(current_routine, fd);
  if (0 <= timeout_ms) {
    current_routine->add_timer(time_point_cast<milliseconds>(high_resolution_clock::now() + milliseconds(timeout_ms)));
  }
  current_routine->commit_event_round();
  current_routine->previous_status_ = routine_status::wait_events;
  current_routine->status_ = routine_status::running;
  return current_routine->happened_type_ == event_type::io_write_panic ||
                 current_routine->happened_type_ == event_type::io_read_panic
             ? code_panic
             : (current_routine->happened_type_ == event_type::timer ? code_timeout : 0);
}

template <int SyscallId> struct boson_classic_syscall {
  template <class... Args>
  static inline decltype(auto) call(int fd, int timeout_ms, Args&&... args) {
    auto return_code = syscall_callable<SyscallId>::call(fd, std::forward<Args>(args)...);
    if (return_code < 0 && (EAGAIN == errno || EWOULDBLOCK == errno)) {
      return_code = wait_readiness<syscall_traits<SyscallId>::is_read>(fd, timeout_ms);
      if (0 == return_code) {
        return_code = syscall_callable<SyscallId>::call(fd, std::forward<Args>(args)...);
      }
    }
    return return_code;
  }
};

ssize_t read(fd_t fd, void* buf, size_t count, int timeout_ms) {
  return boson_classic_syscall<SYS_read>::call(fd, timeout_ms, buf,count);
}

ssize_t write(fd_t fd, const void* buf, size_t count, int timeout_ms) {
  return boson_classic_syscall<SYS_write>::call(fd, timeout_ms, buf,count);
}

socket_t accept(socket_t socket, sockaddr* address, socklen_t* address_len, int timeout_ms) {
  return boson_classic_syscall<SYS_accept>::call(socket, timeout_ms, address, address_len);
}

ssize_t send(socket_t socket, const void* buffer, size_t length, int flags, int timeout_ms) {
  return boson_classic_syscall<SYS_sendto>::call(socket, timeout_ms, buffer, length, flags, nullptr, 0);
}

ssize_t recv(socket_t socket, void* buffer, size_t length, int flags, int timeout_ms) {
  return boson_classic_syscall<SYS_recvfrom>::call(socket, timeout_ms, buffer, length, flags, nullptr, 0);
}

/**
 * Connect system call
 *
 * Its implementation is a bit different from the others
 * the connect call does not have to be made again, and the retured error
 * is not EAGAIN
 */
int connect(socket_t sockfd, const sockaddr *addr, socklen_t addrlen, int timeout_ms) {
  int return_code = syscall_callable<SYS_connect>::call(sockfd, addr, addrlen);
  if (return_code < 0 && errno == EINPROGRESS) {
    return_code = wait_readiness<syscall_traits<SYS_connect>::is_read>(sockfd, timeout_ms);
    if (0 == return_code) {
      socklen_t optlen = 0;
      ::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &return_code, &optlen);
      if (return_code != 0) {
        errno = return_code;
        return_code = -1;
      }
    }
  }
  return return_code;
}

void fd_panic(int fd) {
  current_thread()->engine_proxy_.fd_panic(fd);
}

}  // namespace boson
