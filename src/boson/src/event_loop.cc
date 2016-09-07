#include "boson/event_loop.h"
#include "event_loop_impl.h"

namespace boson {

event_loop::~event_loop() {
}

event_loop::event_loop(event_handler& handler) : loop_impl_{new event_loop_impl{handler}} {
}

int event_loop::register_event(void* data) {
  return loop_impl_->register_event(data);
}

void* event_loop::unregister_event(int event_id) {
  return loop_impl_->unregister_event(event_id);
}

void event_loop::send_event(int event) {
  loop_impl_->send_event(event);
}

void event_loop::request_read(int fd, void* data) {
  loop_impl_->request_read(fd,data);
}

void event_loop::request_write(int fd, void* data) {
  loop_impl_->request_write(fd,data);
}

loop_end_reason event_loop::loop(int max_iter, int timeout_ms) {
  loop_impl_->loop(max_iter, timeout_ms);
}

}  // namespace boson
