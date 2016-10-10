#include "event_loop_impl.h"
#include <sys/eventfd.h>
#include <unistd.h>
#include <cassert>
#include <cstring>
#include "exception.h"
#include "system.h"

namespace boson {

event_loop_impl::event_loop_impl(event_handler& handler)
    : handler_{handler},
      loop_fd_{epoll_create1(0)},
      nb_io_registered_(0),
      trigger_fd_events_{false} {
}

event_loop_impl::~event_loop_impl() {
  ::close(loop_fd_);
}

int event_loop_impl::register_event(void* data) {
  // Creates an eventfd
  int event_fd = ::eventfd(0, 0);

  // Creates users data
  size_t event_id = static_cast<int>(events_data_.allocate());
  event_data& ev_data = events_data_[event_id];
  ev_data.fd = event_fd;
  ev_data.type = event_type::event_fd;
  ev_data.data = data;

  // Register in epoll loop
  epoll_event_t new_event{EPOLLIN, {}};
  new_event.data.u64 = event_id;
  if (events_.size() < events_data_.data().size()) events_.resize(events_data_.data().size());
  int return_code = ::epoll_ctl(loop_fd_, EPOLL_CTL_ADD, event_fd, &new_event);

  if (return_code < 0) {
    throw exception(std::string("Syscall error (epoll_ctl): ") + ::strerror(errno));
  }

  // Casted to int, we dont have to worry about scaling here, int is waaaaay large enough
  return event_id;
}

void event_loop_impl::send_event(int event) {
  auto& data = events_data_[static_cast<size_t>(event)];
  size_t buffer{1};
  ssize_t nb_bytes = ::write(data.fd, &buffer, 8u);
  if (nb_bytes < 0) {
    throw exception(std::string("Syscall error (write): ") + strerror(errno));
  }
  trigger_fd_events_.store(true, std::memory_order_release);
}

int event_loop_impl::register_read(int fd, void* data) {
  // Creates users data
  size_t event_id = static_cast<int>(events_data_.allocate());
  event_data& ev_data = events_data_[event_id];
  ev_data.fd = fd;
  ev_data.events = EPOLLIN;
  ev_data.type = event_type::read;
  ev_data.data = data;

  // Register in epoll loop
  epoll_event_t new_event{ev_data.events, {}};
  new_event.data.u64 = event_id;
  if (events_.size() < events_data_.data().size()) events_.resize(events_data_.data().size());
  int return_code = ::epoll_ctl(loop_fd_, EPOLL_CTL_MOD, fd, &new_event);
  if (return_code < 0 && ENOENT == errno) {
    return_code = ::epoll_ctl(loop_fd_, EPOLL_CTL_ADD, fd, &new_event);
  }
  if (return_code < 0) {
    throw exception(std::string("Syscall error (epoll_ctl): ") + ::strerror(errno));
  }

  ++nb_io_registered_;
  return event_id;
}

int event_loop_impl::register_write(int fd, void* data) {
  // Creates users data
  size_t event_id = static_cast<int>(events_data_.allocate());
  event_data& ev_data = events_data_[event_id];
  ev_data.fd = fd;
  ev_data.events = EPOLLOUT;
  ev_data.type = event_type::write;
  ev_data.data = data;

  // Register in epoll loop
  epoll_event_t new_event{ev_data.events, {}};
  new_event.data.u64 = event_id;
  if (events_.size() < events_data_.data().size()) events_.resize(events_data_.data().size());
  int return_code = ::epoll_ctl(loop_fd_, EPOLL_CTL_MOD, fd, &new_event);
  if (return_code < 0 && ENOENT == errno) {
    return_code = ::epoll_ctl(loop_fd_, EPOLL_CTL_ADD, fd, &new_event);
  }
  if (return_code < 0) {
    throw exception(std::string("Syscall error (epoll_ctl): ") + ::strerror(errno));
  }

  ++nb_io_registered_;
  return event_id;
}

void event_loop_impl::disable(int event_id) {
  auto& event_data = events_data_[event_id];
  epoll_event_t stale_event{0, {}};  // For compatibility with 2.6
  int return_code = ::epoll_ctl(loop_fd_, EPOLL_CTL_MOD, event_data.fd, &stale_event);
  if (return_code < 0) {
    throw exception(std::string("Syscall error (epoll_ctl): ") + ::strerror(errno));
  }
  if (event_data.type != event_type::event_fd) --nb_io_registered_;
}

void event_loop_impl::enable(int event_id) {
  auto& event_data = events_data_[event_id];
  epoll_event_t new_event{event_data.events, event_data.data};  // For compatibility with 2.6
  int return_code = ::epoll_ctl(loop_fd_, EPOLL_CTL_MOD, event_data.fd, &new_event);
  if (return_code < 0) {
    throw exception(std::string("Syscall error (epoll_ctl): ") + ::strerror(errno));
  }
  if (event_data.type != event_type::event_fd) ++nb_io_registered_;
}

void* event_loop_impl::unregister(int event_id) {
  auto& event_data = events_data_[event_id];
  int event_fd = event_data.fd;
  void* data = event_data.data;
  epoll_event_t stale_event{0, {}};  // For compatibility with 2.6
  int return_code = ::epoll_ctl(loop_fd_, EPOLL_CTL_DEL, event_fd, &stale_event);
  if (return_code < 0 && errno != EBADF) {
    throw exception(std::string("Syscall error (epoll_ctl): ") + ::strerror(errno));
  }
  if (event_data.type != event_type::event_fd) --nb_io_registered_;
  events_data_.free(event_id);
  return data;
}

loop_end_reason event_loop_impl::loop(int max_iter, int timeout_ms) {
  bool forever = (-1 == max_iter);
  for (size_t index = 0; index < static_cast<size_t>(max_iter) || forever; ++index) {
    int return_code = 0;
    if (0 != timeout_ms || 0 < nb_io_registered_ ||
        trigger_fd_events_.load(std::memory_order_acquire)) {
      trigger_fd_events_.store(false, std::memory_order_relaxed);
      return_code = ::epoll_wait(loop_fd_, events_.data(), events_.size(), timeout_ms);
      switch (return_code) {
        case 0:
          if (timeout_ms != 0)
            return loop_end_reason::timed_out;
          else
            break;
        case EINTR:
        case EBADF:
        case EFAULT:
        case EINVAL:
          throw exception(std::string("Syscall error (epoll_ctl): ") + ::strerror(errno));
        default:
          break;
      }
    }
    // Success, get on on with dispatching events
    for (int index = 0; index < return_code; ++index) {
      auto& epoll_event = events_[index];
      auto& event_data = events_data_[epoll_event.data.u64];
      switch (event_data.type) {
        case event_type::event_fd: {
          size_t buffer{0};
          ssize_t nb_bytes = ::read(event_data.fd, &buffer, 8u);
          assert(nb_bytes == 8);
          handler_.event(static_cast<int>(epoll_event.data.u64), event_data.data);
        } break;
        case event_type::read: {
          handler_.read(event_data.fd, event_data.data);
        } break;
        case event_type::write: {
          handler_.write(event_data.fd, event_data.data);
        } break;
      }
    }
  }
  return loop_end_reason::max_iter_reached;
}
}
