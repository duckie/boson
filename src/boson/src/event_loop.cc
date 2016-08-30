#include "boson/event_loop.h"
#include "event_loop_impl.h"

namespace boson {

event_loop::event_loop(event_handler& handler, std::size_t max_nb_events) :
  loop_impl_{new event_loop_impl{handler, max_nb_events}}
{
}

int event_loop::register_event(void *data) {
  return loop_impl_->register_event(data);
}

void event_loop::send_event(int event, std::size_t value) {
  loop_impl_->send_event(event,value);
}

void event_loop::loop() {
  loop_impl_->loop();
}

}  // namespace boson
