#ifndef BOSON_EVENTLOOPIMPL_H_
#define BOSON_EVENTLOOPIMPL_H_
#pragma once

#include "event_loop.h"
#include <sys/epoll.h>
#include <vector>


namespace boson {
using epoll_event_t = struct epoll_event;

class event_loop_impl {
  event_handler& handler_;
  int loop_fd_{-1};
  std::vector<epoll_event_t> events_;

 public:
  event_loop_impl(event_handler& handler,std::size_t max_nb_events);

  /**
   * Registers for a long running event listening
   *
   * Unlike read and writes, resgister_event does not work like
   * a one shot request but will live until unregistered
   */
  int register_event(void *data);

  //request_read(int fd, void* data = nullptr);
  //request_write(int fd, void* data = nullptr);
  //
  void send_event(int event, std::size_t value);
};
}


#endif  // BOSON_EVENTLOOP_H_
