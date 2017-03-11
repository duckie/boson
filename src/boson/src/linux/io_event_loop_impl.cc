#include "io_event_loop_impl.h"
#include <sys/eventfd.h>
#include <unistd.h>
#include <cassert>
#include <cstring>
#include "exception.h"
#include "system.h"
#include "logger.h"
#include <sys/resource.h>

template class std::unique_ptr<boson::io_event_loop>;

namespace boson {

io_event_loop::io_event_loop(io_event_handler& handler, int nprocs)
    : handler_{handler},
      loop_fd_{epoll_create1(0)},
      loop_breaker_event_{-1}
{
  loop_breaker_event_ = ::eventfd(0,0);
  epoll_event_t new_event{ EPOLLIN | EPOLLET | EPOLLRDHUP, {}};
  new_event.data.fd = loop_breaker_event_;
  int return_code = ::epoll_ctl(loop_fd_, EPOLL_CTL_ADD, loop_breaker_event_, &new_event);
  if (return_code < 0) {
    throw exception(std::string("Syscall error (epoll_ctl): ") + ::strerror(errno));
  }

  // Prepare event array to max size
  struct rlimit fd_limits {0,0};
  return_code = ::getrlimit(RLIMIT_NOFILE, &fd_limits);
  events_.resize(fd_limits.rlim_cur);
}

io_event_loop::~io_event_loop() {
  ::close(loop_fd_);
  ::close(loop_breaker_event_);
}

void  io_event_loop::interrupt() {
  size_t buffer{1};
  ssize_t nb_bytes = ::write(loop_breaker_event_, &buffer, 8u);
  if (nb_bytes < 0) {
    throw exception(std::string("Syscall error (write): ") + strerror(errno));
  }
}

void io_event_loop::register_fd(int fd, event_data data) {
  epoll_event_t new_event{ EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP, {}};
  new_event.data.ptr = data.ptr;  // Should hover evert type of the union
  int return_code = ::epoll_ctl(loop_fd_, EPOLL_CTL_ADD, fd, &new_event);
  if (return_code < 0) {
    throw exception(std::string("Syscall error (epoll_ctl): ") + ::strerror(errno));
  }
}

void* io_event_loop::unregister(int fd) {
  epoll_event_t new_event{ 0, {}};
  int return_code = ::epoll_ctl(loop_fd_, EPOLL_CTL_DEL, fd, &new_event);
  if (return_code < 0) {
    throw exception(std::string("Syscall error (epoll_ctl): ") + ::strerror(errno));
  }
  pending_commands_.write({ command_type::close_fd, fd });
  return nullptr;
}

void io_event_loop::send_fd_panic(int proc_from,int fd) {
  interrupt();
}

io_loop_end_reason io_event_loop::loop(int max_iter, int timeout_ms) {
  bool forever = (-1 == max_iter);
  bool retry = false;
  for (size_t index = 0; index < static_cast<size_t>(max_iter) || forever || retry; ++index) {
    int return_code = 0;
    retry = false;
    return_code = ::epoll_wait(loop_fd_, events_.data(), events_.size(), timeout_ms);
    switch (return_code) {
      case 0:
        if (timeout_ms != 0)
          return io_loop_end_reason::timed_out;
        else
          break;
      case EINTR:
        //throw exception(std::string("Syscall error (epoll_wait) EINTR : ") + ::strerror(errno));
        // TODO: real cause to be determined, happens under high contention
        retry = true;
        break;
      case EBADF:
        //throw exception(std::string("Syscall error (epoll_wait) EBADF : ") + std::to_string(loop_fd_) + ::strerror(errno));
        retry = true;
        break;
      case EFAULT:
        throw exception(std::string("Syscall error (epoll_wait) EFAULT : ") + ::strerror(errno));
      case EINVAL:
        throw exception(std::string("Syscall error (epoll_wait) EINVAL : ") + ::strerror(errno));
      default:
        break;
    }
    // Success, get on on with dispatching events
    for (int index = 0; index < return_code; ++index) {
      auto& epoll_event = events_[index];
      bool interrupted = epoll_event.events & (EPOLLERR | EPOLLRDHUP);
      if (epoll_event.data.fd != loop_breaker_event_) {
        if (epoll_event.events & EPOLLIN)
          handler_.read(event_data{.ptr = epoll_event.data.ptr}, interrupted ? -EINTR : 0);
        if (epoll_event.events & EPOLLOUT) {
          handler_.write(event_data{.ptr = epoll_event.data.ptr}, interrupted ? -EINTR : 0);
        }
      }
      else {
        assert(epoll_event.events & EPOLLIN);
        size_t buffer{1};
        // TODO: replace with direct sycall
        ::read(loop_breaker_event_, &buffer, 8u);
      }
    }
  }
  return io_loop_end_reason::max_iter_reached;
}
}
