#include "event_loop_impl.h"

namespace boson {

event_loop_impl::event_loop_impl(event_handler& handler, std::size_t max_nb_events) :
  handler_{handler}
{
  events_.resize(max_nb_events);
}

int event_loop_impl::register_event(void *data) {
  return 0;
}

void send_event(int event, std::size_t value) {
}

}
