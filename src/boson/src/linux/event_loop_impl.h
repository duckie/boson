#ifndef BOSON_EVENTLOOPIMPL_H_
#define BOSON_EVENTLOOPIMPL_H_
#pragma once

#include <sys/epoll.h>
#include <atomic>
#include <vector>
#include "event_loop.h"
#include "system.h"
#include "memory/sparse_vector.h"
#include "memory/flat_unordered_set.h"
#include "queues/lcrq.h"

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

  struct broken_loop_event_data {
    int fd;
  };

  event_handler& handler_;

  // epoll fd
  int loop_fd_{-1};

 // Data attached to each event
  memory::sparse_vector<event_data> events_data_;

  // List of event fd events
  memory::flat_unordered_set<int> noio_events_;
  
  /**
   * FD data is a join table of FDs to distinguish events used by more than one routine
   *
   * Its is only relevant for sockets that can can be read/write on the same FD
   */
  std::vector<fd_data> fd_data_;

  // events_ is the array used in the epoll_wait call to store the result
  std::vector<epoll_event_t> events_;

  /**
   * Number of fds registered for read/write event
   *
   * This is used to by pass an epoll_wait call if possible. Event FDs
   * are not considered as a read here, even if they are implemented as
   * such
   */
  size_t nb_io_registered_;

  // A flag to avoid an epoll_wait if possible
  std::atomic<bool> trigger_fd_events_;

  // Private event to implement the fd panic feature
  int loop_breaker_event_;

  // Data used when loop is broken
  queues::lcrq loop_breaker_queue_;
  

  /**
   * Retrieve the event_data for read and write matching this fd
   */
  fd_data& get_fd_data(int fd);

  /**
   * Update the epoll table
   */
  void epoll_update(int fd, fd_data& fddata, bool del_if_no_event);

  /**
   * Update the epoll table
   */
  inline void epoll_update(int fd, bool del_if_no_event) {
    epoll_update(fd, get_fd_data(fd), del_if_no_event);
  }

  /**
   * Signal the event to be dispatched to the handler
   */
  void dispatch_event(int event_id, event_status status);

 public:
  event_loop_impl(event_handler& handler, int nb_procs);
  ~event_loop_impl();
  int register_event(void* data);
  void send_event(int event);
  int register_read(int fd, void* data);
  int register_write(int fd, void* data);
  void disable(int event_id);
  void enable(int event_it);
  void* unregister(int event_id);
  void send_fd_panic(int proc_from, int fd);
  loop_end_reason loop(int max_iter = -1, int timeout_ms = -1);
};
}

#endif  // BOSON_EVENTLOOP_H_
