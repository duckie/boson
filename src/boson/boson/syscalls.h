#ifndef BOSON_SYSCALLS_H_
#define BOSON_SYSCALLS_H_

#include <sys/socket.h>
#include <chrono>
#include <cstdint>
#include "system.h"

namespace boson {

static constexpr int code_ok = -100;
static constexpr int code_timeout = -102;
static constexpr int code_panic = -101;

/**
 * Gives back control to the scheduler
 *
 * This is useful in CPU intensive routines not to block
 * other routines from being executed as well.
 */
void yield();

/**
 * Suspends the routine for the given duration
 */
void sleep(std::chrono::milliseconds duration);

/**
 * Suspends the routine until the fd is ready for a syscall
 */
int wait_readiness(fd_t fd, bool read, int timeout_ms = -1);

/**
 * Suspends the routine until the fd is read for read/recv/accept
 */
inline int wait_read_readiness(fd_t fd, int timeout_ms = -1) {
  return wait_readiness(fd, true, timeout_ms);
}

/**
 * Suspends the routine until the fd is read for write/send
 */
inline int wait_write_readiness(fd_t fd, int timeout_ms = -1) {
  return wait_readiness(fd, false, timeout_ms);
}

/**
 * Boson equivalent to POSIX read system call
 */
ssize_t read(fd_t fd, void *buf, size_t count, int timeout_ms = -1);

inline ssize_t read(fd_t fd, void *buf, size_t count, std::chrono::milliseconds timeout) {
  return read(fd, buf, count, timeout.count());
}

/**
 * Boson equivalent to POSIX write system call
 */
ssize_t write(fd_t fd, const void *buf, size_t count, int timeout_ms = -1);

inline ssize_t write(fd_t fd, const void *buf, size_t count, std::chrono::milliseconds timeout) {
  return write(fd, buf, count, timeout.count());
}

socket_t accept(socket_t socket, sockaddr *address, socklen_t *address_len, int timeout_ms = -1);

inline socket_t accept(socket_t socket, sockaddr *address, socklen_t *address_len, std::chrono::milliseconds timeout) {
    return accept(socket, address, address_len, timeout.count());
}

int connect(socket_t sockfd, const sockaddr *addr, socklen_t addrlen, int timeout_ms = -1);
inline int connect(socket_t sockfd, const sockaddr *addr, socklen_t addrlen, std::chrono::milliseconds timeout) {
  return connect(sockfd, addr, addrlen, timeout.count());
}

ssize_t send(socket_t socket, const void *buffer, size_t length, int flags, int timeout_ms = -1);

inline ssize_t send(socket_t socket, const void *buffer, size_t length, int flags, std::chrono::milliseconds timeout) {
  return send(socket, buffer, length, flags, timeout.count());
}

ssize_t recv(socket_t socket, void *buffer, size_t length, int flags, int timeout_ms = -1);

inline ssize_t recv(socket_t socket, void *buffer, size_t length, int flags, std::chrono::milliseconds timeout) {
  return recv(socket, buffer, length, flags, timeout.count());
}

void fd_panic(int fd);

}  // namespace boson

#endif  // BOSON_SYSCALLS_H_
