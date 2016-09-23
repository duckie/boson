#include "boson/event_loop.h"
#include <unistd.h>
#include <thread>
#include "catch.hpp"

struct handler01 : public boson::event_handler {
  int last_id{0};
  int last_fd{-1};
  void* last_data{nullptr};

  void event(int event_id, void* data) override {
    last_id = event_id;
    last_fd = -1;
    last_data = data;
  }
  void read(int fd, void* data) override {
    last_id = -1;
    last_fd = fd;
    last_data = data;
  }
  void write(int fd, void* data) override {
    last_id = -1;
    last_fd = fd;
    last_data = data;
  }
};

TEST_CASE("Event Loop - Event notification", "[eventloop][notif]") {
  handler01 handler_instance;

  boson::event_loop loop(handler_instance);
  int event_id = loop.register_event(nullptr);

  std::thread t1{[&loop]() { loop.loop(1); }};

  loop.send_event(event_id);
  t1.join();

  CHECK(handler_instance.last_id == event_id);
  CHECK(handler_instance.last_data == nullptr);
}

TEST_CASE("Event Loop - FD Read/Write", "[eventloop][read/write]") {
  handler01 handler_instance;
  int pipe_fds[2];
  ::pipe(pipe_fds);

  boson::event_loop loop(handler_instance);
  loop.register_read(pipe_fds[0], nullptr);
  loop.register_write(pipe_fds[1], nullptr);

  loop.loop(1);
  CHECK(handler_instance.last_fd == pipe_fds[1]);
  CHECK(handler_instance.last_data == nullptr);

  size_t data{1};
  ::write(pipe_fds[1], &data, sizeof(size_t));

  loop.loop(1);
  CHECK(handler_instance.last_fd == pipe_fds[0]);
  CHECK(handler_instance.last_data == nullptr);
}
