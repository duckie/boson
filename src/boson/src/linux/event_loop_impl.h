#ifndef BOSON_EVENTLOOPIMPL_H_
#define BOSON_EVENTLOOPIMPL_H_
#pragma once

#include <sys/epoll.h>
#include <vector>
#include "event_loop.h"
#include "memory/sparse_vector.h"

namespace boson {
using epoll_event_t = struct epoll_event;

class event_loop_impl {
  struct event_data {
    int fd;
    void* data;
  };

  event_handler& handler_;
  int loop_fd_{-1};
  memory::sparse_vector<event_data> events_data_;
  std::vector<epoll_event_t> events_;

 public:
  event_loop_impl(event_handler& handler);

  /**
   * Registers for a long running event listening
   *
   * Unlike read and writes, resgister_event does not work like
   * a one shot request but will live until unregistered
   */
  int register_event(void* data);

  /**
   * Unregister the vent and give back its data
   */
  void* unregister_event(int event_id);

  // request_read(int fd, void* data = nullptr);
  // request_write(int fd, void* data = nullptr);
  //
  void send_event(int event);

  void loop(int max_iter = -1);
};
}

#endif  // BOSON_EVENTLOOP_H_
