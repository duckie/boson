#ifndef BOSON_SYSCALLS_H_
#define BOSON_SYSCALLS_H_

#include <sys/socket.h>
#include <chrono>
#include <cstdint>
#include <utility>
#include "system.h"
#include "std/experimental/chrono.h"

namespace boson {

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

void usleep(std::chrono::microseconds duration_ms);
void nanosleep(std::chrono::nanoseconds duration_ns);

template <class T_Rep, class T_Period>
void sleep(std::chrono::duration<T_Rep, T_Period> const& duration) {
  using namespace std::chrono;
  usleep(duration_cast<microseconds>(duration));
}

/**
 * Suspends the routine until the fd is ready for a syscall
 */
template <bool IsARead, bool HasTimer>
int wait_readiness(fd_t fd, int timeout_ms = 0);

template <bool HasTimer>
socket_t accept_impl(socket_t socket, sockaddr *address, socklen_t *address_len, int timout_ms);

// Proper C++11 versions

ssize_t read(fd_t fd, void *buf, size_t count, std::chrono::milliseconds const &timeout);
template <class T_Rep, class T_Period>
inline ssize_t read(fd_t fd, void *buf, size_t count,
                    std::chrono::duration<T_Rep, T_Period> const &timeout) {
  return read(fd, buf, count, experimental::chrono::ceil<std::chrono::milliseconds>(timeout));
}

ssize_t write(fd_t fd, const void *buf, size_t count, std::chrono::milliseconds const &timeout);
template <class T_Rep, class T_Period>
inline ssize_t write(fd_t fd, const void *buf, size_t count,
                     std::chrono::duration<T_Rep, T_Period> const &timeout) {
  return write(fd, buf, count, experimental::chrono::ceil<std::chrono::milliseconds>(timeout));
}

socket_t accept(socket_t socket, sockaddr *address, socklen_t *address_len,
                std::chrono::milliseconds const &timeout_ms);
template <class T_Rep, class T_Period>
inline socket_t accept(socket_t socket, sockaddr *address, socklen_t *address_len,
                       std::chrono::duration<T_Rep, T_Period> const &timeout) {
  return accept(socket, address, address_len,
                experimental::chrono::ceil<std::chrono::milliseconds>(timeout));
}

int connect(socket_t sockfd, const sockaddr *addr, socklen_t addrlen,
            std::chrono::milliseconds const &timeout);
template <class T_Rep, class T_Period>
inline int connect(socket_t sockfd, const sockaddr *addr, socklen_t addrlen,
                   std::chrono::duration<T_Rep, T_Period> const &timeout) {
  return connect(sockfd, addr, addrlen,
                 experimental::chrono::ceil<std::chrono::milliseconds>(timeout));
}

ssize_t send(socket_t socket, const void *buffer, size_t length, int flags,
             std::chrono::milliseconds const &timeout);
template <class T_Rep, class T_Period>
inline ssize_t send(socket_t socket, const void *buffer, size_t length, int flags,
                    std::chrono::duration<T_Rep, T_Period> const &timeout) {
  return send(socket, buffer, length, flags,
              experimental::chrono::ceil<std::chrono::milliseconds>(timeout));
}

ssize_t recv(socket_t socket, void *buffer, size_t length, int flags,
             std::chrono::milliseconds const &timeout);
template <class T_Rep, class T_Period>
inline ssize_t recv(socket_t socket, void *buffer, size_t length, int flags,
                    std::chrono::duration<T_Rep, T_Period> const &timeout) {
  return recv(socket, buffer, length, flags, experimental::chrono::ceil<std::chrono::milliseconds>(timeout));
}

// Boson equivalents to POSIX systemcalls

unsigned int sleep(unsigned int duration_seconds);
int usleep(useconds_t duration_ms);
int nanosleep(const struct timespec* rqtp, struct timespec* rmtp);

fd_t open(const char *pathname, int flags);
fd_t open(const char *pathname, int flags, mode_t mode);
fd_t creat(const char *pathname, mode_t mode);
int pipe(fd_t (&fds)[2]);
int pipe2(fd_t (&fds)[2], int flags);
socket_t socket(int domain, int type, int protocol);

ssize_t read(fd_t fd, void *buf, size_t count);
ssize_t write(fd_t fd, const void *buf, size_t count);
socket_t accept(socket_t socket, sockaddr *address, socklen_t *address_len);
int connect(socket_t sockfd, const sockaddr *addr, socklen_t addrlen);
ssize_t send(socket_t socket, const void *buffer, size_t length, int flags);
ssize_t recv(socket_t socket, void *buffer, size_t length, int flags);

// Versions with C++11 durations

//ssize_t read(fd_t fd, void *buf, size_t count, std::chrono::milliseconds timeout);
//
//ssize_t write(fd_t fd, const void *buf, size_t count, std::chrono::milliseconds timeout) {
  //return write(fd, buf, count, timeout.count());
//}
//
//socket_t accept(socket_t socket, sockaddr *address, socklen_t *address_len, std::chrono::milliseconds timeout) {
    //return accept(socket, address, address_len, timeout.count());
//}
//
//int connect(socket_t sockfd, const sockaddr *addr, socklen_t addrlen, std::chrono::milliseconds timeout) {
  //return connect(sockfd, addr, addrlen, timeout.count());
//}
//
//
//ssize_t send(socket_t socket, const void *buffer, size_t length, int flags, std::chrono::milliseconds timeout) {
  //return send(socket, buffer, length, flags, timeout.count());
//}
//
//ssize_t recv(socket_t socket, void *buffer, size_t length, int flags, std::chrono::milliseconds timeout) {
  //return recv(socket, buffer, length, flags, timeout.count());
//}

int close(int fd);

}  // namespace boson

#endif  // BOSON_SYSCALLS_H_
