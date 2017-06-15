#include "boson/syscalls.h"
#include "boson/exception.h"
#include "boson/internal/routine.h"
#include "boson/internal/thread.h"
#include "boson/syscall_traits.h"
#include "boson/engine.h"
#include "boson/std/experimental/chrono.h"

namespace boson {

using namespace internal;

void yield() {
  thread* this_thread = current_thread();
  routine* current_routine = this_thread->running_routine();
  current_routine->status_ = routine_status::yielding;
  this_thread->context() = jump_fcontext(this_thread->context().fctx, nullptr);
  current_routine->previous_status_ = routine_status::yielding;
  current_routine->status_ = routine_status::running;
}

void nanosleep(std::chrono::nanoseconds duration) {
  using namespace std::chrono;
  usleep(duration_cast<microseconds>(duration));
}

void usleep(std::chrono::microseconds duration) {
  using namespace std::chrono;
  thread* this_thread = current_thread();
  routine* current_routine = this_thread->running_routine();
  current_routine->start_event_round();
  current_routine->add_timer(
      experimental::chrono::ceil<milliseconds>(high_resolution_clock::now() + duration));
  current_routine->commit_event_round();
  current_routine->previous_status_ = routine_status::wait_events;
  current_routine->status_ = routine_status::running;
}

unsigned int sleep(unsigned int duration_seconds) {
  using namespace std::chrono;
  usleep(duration_cast<milliseconds>(seconds(duration_seconds)));
  return 0;
}

int usleep(useconds_t duration_ms) {
  usleep(std::chrono::microseconds(duration_ms));
  return 0;
}

int nanosleep(const struct timespec* rqtp, struct timespec* rmtp) {
  using namespace std::chrono;
  usleep(
      experimental::chrono::ceil<microseconds>(seconds(rqtp->tv_sec) + nanoseconds(rqtp->tv_nsec)));
  if (rmtp) {
    rmtp->tv_sec = 0;
    rmtp->tv_nsec = 0;
  }
  return 0;
}

template <bool IsARead, bool HasTimer>
int wait_readiness(fd_t fd, int timeout_ms) {
  using namespace std::chrono;
  thread* this_thread = current_thread();
  routine* current_routine = this_thread->running_routine();
  current_routine->start_event_round();
  add_event<IsARead>::apply(current_routine, fd);
  add_timer<HasTimer>::apply(current_routine, timeout_ms);
  current_routine->commit_event_round();
  current_routine->previous_status_ = routine_status::wait_events;
  current_routine->status_ = routine_status::running;

  int return_code = 0;
  if (current_routine->happened_rc_  < 0) {
    errno = -current_routine->happened_rc_;
    return_code = -1;
  }
  return return_code;
}

template <int SyscallId> struct boson_classic_syscall {
  template <bool HasTimer, class... Args>
  static inline decltype(auto) call(int fd, int timeout_ms, Args&&... args) {
    (void)timeout_ms;  // For "unused" warning
    auto return_code = syscall_callable<SyscallId>::call(fd, std::forward<Args>(args)...);
    int nb_retry = 0;
    while(return_code < 0 && (EAGAIN == errno || EWOULDBLOCK == errno)) {
      ++nb_retry;
      return_code = wait_readiness<syscall_traits<SyscallId>::is_read,HasTimer>(fd, timeout_ms);
      if (0 == return_code) {
        return_code = syscall_callable<SyscallId>::call(fd, std::forward<Args>(args)...);
      }
    }
    if (1 < nb_retry) {
      debug::log("Syscall {} finished with {} retries.", SyscallId, nb_retry);
    }
    return return_code;
  }
};

fd_t open(const char *pathname, int flags) {
  fd_t fd = ::syscall(SYS_open,pathname, flags | O_NONBLOCK, 0755);
  return fd;
}

fd_t open(const char *pathname, int flags, mode_t mode) {
  fd_t fd = ::syscall(SYS_open,pathname, flags | O_NONBLOCK, mode);
  return fd;
}

fd_t creat(const char *pathname, mode_t mode) {
  fd_t fd = ::syscall(SYS_open,pathname, O_CREAT | O_WRONLY | O_TRUNC| O_NONBLOCK, mode);
  return fd;
}

fd_t pipe(fd_t (&fds)[2]) {
  int rc = ::syscall(SYS_pipe2, fds, O_NONBLOCK);
  return rc;
}

fd_t pipe2(fd_t (&fds)[2], int flags) {
  int rc = ::syscall(SYS_pipe2, fds, flags | O_NONBLOCK);
  return rc;
}

socket_t socket(int domain, int type, int protocol) {
  socket_t socket = ::syscall(SYS_socket, domain, type | SOCK_NONBLOCK, protocol);
  return socket;
}

ssize_t read(fd_t fd, void *buf, size_t count) {
  return boson_classic_syscall<SYS_read>::call<false>(fd, -1, buf,count);
}

ssize_t write(fd_t fd, const void *buf, size_t count) {
  return boson_classic_syscall<SYS_write>::call<false>(fd, -1, buf,count);
}

template <bool HasTimer>
socket_t accept_impl(socket_t socket, sockaddr *address, socklen_t *address_len, int timout_ms) {
  socket_t new_socket = boson_classic_syscall<SYS_accept>::call<HasTimer>(socket, timout_ms, address, address_len);
  if (0 <= new_socket) {
    ::fcntl(new_socket, F_SETFL, ::fcntl(new_socket, F_GETFD) | O_NONBLOCK);
  }
  return new_socket;
}

socket_t accept(socket_t socket, sockaddr *address, socklen_t *address_len) {
  return accept_impl<false>(socket, address, address_len, -1);
}

ssize_t send(socket_t socket, const void *buffer, size_t length, int flags) {
  return boson_classic_syscall<SYS_sendto>::call<false>(socket, -1, buffer, length, flags, nullptr, nullptr);
}

ssize_t recv(socket_t socket, void *buffer, size_t length, int flags) {
  return boson_classic_syscall<SYS_recvfrom>::call<false>(socket, -1, buffer, length, flags, nullptr, nullptr);
}

ssize_t read(fd_t fd, void* buf, size_t count, std::chrono::milliseconds const& timeout_ms) {
  return boson_classic_syscall<SYS_read>::call<true>(fd, static_cast<int>(timeout_ms.count()), buf,count);
}

ssize_t write(fd_t fd, const void* buf, size_t count, std::chrono::milliseconds const& timeout_ms) {
  return boson_classic_syscall<SYS_write>::call<true>(fd, static_cast<int>(timeout_ms.count()), buf,count);
}

socket_t accept(socket_t socket, sockaddr* address, socklen_t* address_len, std::chrono::milliseconds const& timeout_ms) {
  return accept_impl<true>(socket, address, address_len, static_cast<int>(timeout_ms.count()));
}

ssize_t send(socket_t socket, const void* buffer, size_t length, int flags, std::chrono::milliseconds const& timeout_ms) {
  return boson_classic_syscall<SYS_sendto>::call<true>(socket, static_cast<int>(timeout_ms.count()), buffer, length, flags, nullptr, nullptr);
}

ssize_t recv(socket_t socket, void* buffer, size_t length, int flags, std::chrono::milliseconds const& timeout_ms) {
  return boson_classic_syscall<SYS_recvfrom>::call<true>(socket, static_cast<int>(timeout_ms.count()), buffer, length, flags, nullptr, nullptr);
}

template <bool HasTimer> inline int connect_impl(socket_t sockfd, const sockaddr* addr, socklen_t addrlen, int timeout_ms) {
  int return_code = syscall_callable<SYS_connect>::call(sockfd, addr, addrlen);
  if (return_code < 0 && errno == EINPROGRESS) {
    return_code = wait_readiness<syscall_traits<SYS_connect>::is_read, HasTimer>(sockfd, timeout_ms);
    if (0 == return_code) {
      socklen_t optlen = sizeof(return_code);
      if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &return_code, &optlen) < 0)
        throw boson::exception(std::string("boson::connect getsockopt failed (") + ::strerror(errno) + ")");
      if (return_code != 0) {
        errno = return_code;
        return_code = -1;
      }
    }
  }
  return return_code;
}

/**
 * Connect system call
 *
 * Its implementation is a bit different from the others
 * the connect call does not have to be made again, and the retured error
 * is not EAGAIN
 */
int connect(socket_t sockfd, const sockaddr* addr, socklen_t addrlen) {
  return connect_impl<false>(sockfd, addr, addrlen, 0);
}

/**
 * Connect system call
 *
 * Its implementation is a bit different from the others
 * the connect call does not have to be made again, and the retured error
 * is not EAGAIN
 */
int connect(socket_t sockfd, const sockaddr* addr, socklen_t addrlen, std::chrono::milliseconds const& timeout_ms) {
  return connect_impl<true>(sockfd, addr, addrlen, static_cast<int>(timeout_ms.count()));
}

int close(fd_t fd) {
  //current_thread()->engine_proxy_.get_engine().event_loop().signal_fd_closed(fd);
  current_thread()->event_loop_.signal_fd_closed(fd);
  int rc = syscall_callable<SYS_close>::call(fd);
  auto current_errno = errno;
  //current_thread()->unregister_fd(fd);
  errno = current_errno;
  return rc;
}

}  // namespace boson
