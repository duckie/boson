#include "event_loop.h"
#include "event_loop_impl.h"

namespace boson {
template <class LoopImpl>
event_loop<LoopImpl>::event_loop(event_handler& handler, int nprocs)
    : loop_impl_{new LoopImpl(handler, nprocs)} {
}

template <class LoopImpl>
event_loop<LoopImpl>::~event_loop() {}

template <class LoopImpl>
int event_loop<LoopImpl>::register_event(void* data) {
  return loop_impl_->register_event(data);
}

template <class LoopImpl>
void* event_loop<LoopImpl>::get_data(int event_id) {
  return loop_impl_->get_data(event_id);
}

template <class LoopImpl>
std::tuple<int, int> event_loop<LoopImpl>::get_events(int fd) {
  return loop_impl_->get_events(fd);
}

template <class LoopImpl>
void event_loop<LoopImpl>::send_event(int event) {
  loop_impl_->send_event(event);
}

template <class LoopImpl>
int event_loop<LoopImpl>::register_read(int fd, void* data) {
  return loop_impl_->register_read(fd, data);
}

template <class LoopImpl>
int event_loop<LoopImpl>::register_write(int fd, void* data) {
  return loop_impl_->register_write(fd, data);
}

template <class LoopImpl>
void event_loop<LoopImpl>::disable(int event_id) {
  loop_impl_->disable(event_id);
}

template <class LoopImpl>
void event_loop<LoopImpl>::enable(int event_id) {
  loop_impl_->disable(event_id);
}

template <class LoopImpl>
void* event_loop<LoopImpl>::unregister(int event_id) {
  return loop_impl_->unregister(event_id);
}

template <class LoopImpl>
void event_loop<LoopImpl>::send_fd_panic(int proc_from, int fd) {
  loop_impl_->send_fd_panic(proc_from, fd);
}

template <class LoopImpl>
loop_end_reason event_loop<LoopImpl>::loop(int max_iter, int timeout_ms) {
  return loop_impl_->loop(max_iter, timeout_ms);
}
}

template class std::unique_ptr<boson::event_loop_impl>;
template class boson::event_loop<boson::event_loop_impl>;
