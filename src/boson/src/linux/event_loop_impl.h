#ifndef BOSON_EVENTLOOPIMPL_H_
#define BOSON_EVENTLOOPIMPL_H_
#pragma once

#include <sys/epoll.h>
#include <atomic>
#include <vector>
#include "event_loop.h"
#include "memory/sparse_vector.h"

namespace boson {
using epoll_event_t = struct epoll_event;

/**
 * Event loop Linux implementation
 *
 * Refer to the event loop interface for member functions
 * meaning
 */
class event_loop_impl {
  enum class event_type { event_fd, read, write };

  struct event_data {
    int fd;
    uint32_t events;
    event_type type;
    void* data;
  };

  event_handler& handler_;
  int loop_fd_{-1};
  memory::sparse_vector<event_data> events_data_;
  std::vector<epoll_event_t> events_;
  size_t nb_io_registered_;
  std::atomic<bool> trigger_fd_events_;  // Only used for fd_event to bypass epoll

 public:
  event_loop_impl(event_handler& handler);
  ~event_loop_impl();
  int register_event(void* data);
  void send_event(int event);
  int register_read(int fd, void* data);
  int register_write(int fd, void* data);
  void disable(int event_id);
  void enable(int event_it);
  void* unregister(int event_id);
  loop_end_reason loop(int max_iter = -1, int timeout_ms = -1);
};
}

#endif  // BOSON_EVENTLOOP_H_
