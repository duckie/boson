#include "boson/syscalls.h"
#include "boson/internal/routine.h"
#include "boson/internal/thread.h"

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

int wait_readiness(fd_t fd, bool read) {
  thread* this_thread = current_thread();
  routine* current_routine = this_thread->running_routine();
  routine_io_event new_event{fd, -1, false, false};
  current_routine->start_event_round();
  if (read) {
    current_routine->add_read(fd);
  }
  else {
    current_routine->add_write(fd);
  }
  current_routine->commit_event_round();
  current_routine->previous_status_ = routine_status::wait_events;
  current_routine->status_ = routine_status::running;
  return current_routine->happened_type_ == event_type::io_write_panic ||
                 current_routine->happened_type_ == event_type::io_read_panic
             ? code_panic
             : 0;
}

ssize_t read(fd_t fd, void* buf, size_t count) {
  int return_code = wait_read_readiness(fd);
  return return_code ? return_code : ::read(fd, buf, count);
}

ssize_t write(fd_t fd, const void* buf, size_t count) {
  int return_code = wait_write_readiness(fd);
  return return_code ? return_code : ::write(fd, buf, count);
}

socket_t accept(socket_t socket, sockaddr* address, socklen_t* address_len) {
  int return_code = wait_read_readiness(socket);
  return return_code ? return_code : ::accept(socket, address, address_len);
}

size_t send(socket_t socket, const void* buffer, size_t length, int flags) {
  int return_code = wait_write_readiness(socket);
  return return_code ? return_code : ::send(socket, buffer, length, flags);
}

ssize_t recv(socket_t socket, void* buffer, size_t length, int flags) {
  int return_code = wait_read_readiness(socket);
  return return_code ? return_code : ::recv(socket, buffer, length, flags);
}

void fd_panic(int fd) {
  current_thread()->engine_proxy_.fd_panic(fd);
}

}  // namespace boson
