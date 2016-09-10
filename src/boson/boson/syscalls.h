#ifndef BOSON_SYSCALLS_H_
#define BOSON_SYSCALLS_H_

#include <cstdint>
#include <chrono>

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

}  // namespace boson

#endif  // BOSON_SYSCALLS_H_
