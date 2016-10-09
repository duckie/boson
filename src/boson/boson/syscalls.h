#ifndef BOSON_SYSCALLS_H_
#define BOSON_SYSCALLS_H_

#include <sys/socket.h>
#include <chrono>
#include <cstdint>

namespace boson {

/**
 * Gives back control to the scheduler
 *
 * This is useful in CPU intensive routines not to block
 * other routines from be executed as well.
 */
void yield();

/**
 * Suspends the routine for the given duration
 */
void sleep(std::chrono::milliseconds duration);

/**
 * Boson equivalent to POSIX read system call
 */
ssize_t read(int fd, void *buf, size_t count);

/**
 * Boson equivalent to POSIX write system call
 */
ssize_t write(int fd, const void *buf, size_t count);

int accept(int socket, struct sockaddr *address, socklen_t *address_len);
size_t send(int socket, const void *buffer, size_t length, int flags);
ssize_t recv(int socket, void *buffer, size_t length, int flags);

}  // namespace boson

#endif  // BOSON_SYSCALLS_H_
