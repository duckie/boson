#ifndef BOSON_EVENTLOOPIMPL_H_
#define BOSON_EVENTLOOPIMPL_H_
#pragma once

#include <sys/epoll.h>
#include <atomic>
#include <vector>
#include "event_loop.h"
#include "system.h"
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
    //uint32_t events;
    event_type type;
    void* data;
  };

  struct fd_data {
    int idx_read;
    int idx_write;

    inline fd_data() : idx_read{-1}, idx_write{-1} {}
    inline fd_data(int r, int w) : idx_read{r}, idx_write{w} {}
  };

  event_handler& handler_;
  int loop_fd_{-1};
  memory::sparse_vector<event_data> events_data_;
  
  /**
   * FD data is a join table of FDs to distinguish events used by more than one routine
   *
   * Its is only relevant for sockets that can can be read/write on the same FD
   */
  std::vector<fd_data> fd_data_;

  /**
   * events_ is the array used in the epoll call to store the result
   */
  std::vector<epoll_event_t> events_;
  size_t nb_io_registered_;
  std::atomic<bool> trigger_fd_events_;  // Only used for fd_event to bypass epoll

  fd_data& get_fd_data(int fd);

  void epoll_update(int fd, fd_data& fddata, bool del_if_no_event);
  inline void epoll_update(int fd, bool del_if_no_event) {
    epoll_update(fd, get_fd_data(fd), del_if_no_event);
  }

  void dispatch_event(int event_id);

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
