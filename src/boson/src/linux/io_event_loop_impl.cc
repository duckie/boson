#include "io_event_loop_impl.h"
#include <sys/eventfd.h>
#include <unistd.h>
#include <cassert>
#include <cstring>
#include "exception.h"
#include "system.h"
#include "logger.h"

template class std::unique_ptr<boson::io_event_loop>;

namespace boson {

io_event_loop::io_event_loop(io_event_handler& handler, int nprocs)
    : handler_{handler},
      loop_fd_{epoll_create1(0)},
      nb_io_registered_(0),
      trigger_fd_events_{false},
      loop_breaker_event_{-1},
      loop_breaker_queue_{nprocs+1} {
  loop_breaker_event_ = register_event(nullptr);
}

io_event_loop::~io_event_loop() {
  ::close(loop_fd_);
}

int io_event_loop::register_event(void* data) {
  // Creates an eventfd
  int event_fd = ::eventfd(0, 0);
  epoll_event_t new_event{ EPOLLIN | EPOLLET | EPOLLRDHUP, {}};
  new_event.data.fd = event_fd;
  int return_code = ::epoll_ctl(loop_fd_, EPOLL_CTL_ADD, event_fd, &new_event);
  if (return_code < 0) {
    throw exception(std::string("Syscall error (epoll_ctl): ") + ::strerror(errno));
  }
  return event_fd;
}

void io_event_loop::send_event(int fd) {
  size_t buffer{1};
  // TODO: replace with direct sycall
  ssize_t nb_bytes = ::write(fd, &buffer, 8u);
  if (nb_bytes < 0) {
    throw exception(std::string("Syscall error (write): ") + strerror(errno));
  }
  trigger_fd_events_.store(true, std::memory_order_release);
}

void io_event_loop::register_fd(int fd, void* data) {
  epoll_event_t new_event{ EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP, {}};
  new_event.data.fd = fd;
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
  return nullptr;
}

void io_event_loop::send_fd_panic(int proc_from,int fd) {
  loop_breaker_queue_.write(proc_from+1, new broken_loop_event_data{fd});
  send_event(loop_breaker_event_);
}

io_loop_end_reason io_event_loop::loop(int max_iter, int timeout_ms) {
  bool forever = (-1 == max_iter);
  bool retry = false;
  for (size_t index = 0; index < static_cast<size_t>(max_iter) || forever || retry; ++index) {
    int return_code = 0;
    retry = false;
    if (0 != timeout_ms || 0 < nb_io_registered_ ||
        trigger_fd_events_.load(std::memory_order_acquire)) {
      trigger_fd_events_.store(false, std::memory_order_relaxed);
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
        if (epoll_event.events & EPOLLIN)
          handler_.read(epoll_event.data.fd, nullptr, interrupted ? -EINTR : 0);
        if (epoll_event.events & EPOLLOUT)
          handler_.write(epoll_event.data.fd, nullptr, interrupted ? -EINTR : 0);
      }
    }
  }
  return io_loop_end_reason::max_iter_reached;
}
}
