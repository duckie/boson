#ifndef BOSON_IOEVENTLOOPIMPL_H_
#define BOSON_IOEVENTLOOPIMPL_H_
#pragma once

#include <sys/epoll.h>
#include <atomic>
#include <vector>
#include "io_event_loop.h"
#include "system.h"
#include "memory/sparse_vector.h"
#include "memory/flat_unordered_set.h"
#include "queues/simple.h"
#include "queues/mpsc.h"

namespace boson {
using epoll_event_t = struct epoll_event;

/**
 * Event loop Linux implementation
 *
 * Refer to the event loop interface for member functions
 * meaning
 */
class io_event_loop {

  enum class command_type {
    close_fd
  };

  struct command {
    command_type type;
    int fd;
  };

  struct broken_loop_event_data {
    int fd;
  };

  io_event_handler& handler_;

  // epoll fd
  int loop_fd_{-1};

  // events_ is the array used in the epoll_wait call to store the result
  std::vector<epoll_event_t> events_;

  /**
   * Number of fds registered for read/write event
   *
   * This is used to by pass an epoll_wait call if possible. Event FDs
   * are not considered as a read here, even if they are implemented as
   * such
   */
  //size_t nb_io_registered_;

  // A flag to avoid an epoll_wait if possible
  //std::atomic<bool> trigger_fd_events_;

  // Private event to implement the fd panic feature
  int loop_breaker_event_;

  // Data used when loop is broken
  //queues::simple_void_queue loop_breaker_queue_;
  queues::mpsc<command> pending_commands_;

  /**
   * Signal the event to be dispatched to the handler
   */
  void dispatch_event(int event_id, event_status status);

 public:
  io_event_loop(io_event_handler& handler, int nb_procs);
  ~io_event_loop();

  void interrupt();
  void register_fd(int fd);
  void* unregister(int fd);
  void* get_data(int event_id);
  void send_event(int event);
  void send_fd_panic(int proc_from, int fd);
  io_loop_end_reason loop(int max_iter = -1, int timeout_ms = -1);

  static size_t get_max_fds();
};
}

#endif  // BOSON_IOEVENTLOOPIMPL_H_
