#include "event_loop_impl.h"
#include <sys/eventfd.h>

namespace boson {

event_loop_impl::event_loop_impl(event_handler& handler, std::size_t max_nb_events) :
  handler_{handler}
{
  events_data_.resize(max_nb_events);
  events_.resize(max_nb_events);
}

int event_loop_impl::register_event(void *data) {
  // Creates an evenftff
  int event_id = ::eventfd(0,0);
  epoll_event_t new_event;
  new_event.events = EPOLLIN;


  return event_id;
}

void event_loop_impl::send_event(int event, std::size_t value) {
  
}

}
