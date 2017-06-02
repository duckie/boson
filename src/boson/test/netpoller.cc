#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <thread>
#include <cstring>
#include "boson/exception.h"
#include "boson/system.h"
#include "boson/internal/netpoller.h"
#include "catch.hpp"

using namespace boson;

namespace {
struct handler01 : public internal::net_event_handler<int> {
  int last_read_fd{-1};
  int last_write_fd{-1};
  event_status last_status {};

  void read(int fd, int data, event_status status) override {
    last_read_fd = data;
    last_status = status;
  }
  void write(int fd, int data, event_status status) override {
    last_write_fd = data;
    last_status = status;
  }
  void callback() override {};
};
}

TEST_CASE("Netpoller - FD Read/Write", "[netpoller][read/write]") {
  handler01 handler_instance;
  int pipe_fds[2];
  ::pipe(pipe_fds);
  ::fcntl(pipe_fds[0], F_SETFL, ::fcntl(pipe_fds[0], F_GETFD) | O_NONBLOCK);
  ::fcntl(pipe_fds[1], F_SETFL, ::fcntl(pipe_fds[1], F_GETFD) | O_NONBLOCK);

  boson::internal::netpoller<int> loop(handler_instance);

  // Test 1
  std::thread t1{[&loop]() { 
    loop.wait();
  }};
  loop.signal_new_fd(pipe_fds[0]);
  loop.signal_new_fd(pipe_fds[1]);
  loop.register_read(pipe_fds[0], 1);
  loop.register_write(pipe_fds[1], 2);
  t1.join();
  CHECK(handler_instance.last_write_fd == 2);
  CHECK(handler_instance.last_status == 0);

  // Test 2
  std::thread t2{[&loop]() { 
    loop.wait();
  }};
  size_t data{1};
  ::write(pipe_fds[1], &data, sizeof(size_t));
  t2.join();
  CHECK(handler_instance.last_read_fd == 1);
}

TEST_CASE("Netpoller - FD Read/Write same FD", "[netpoller][read/write]") {
#ifdef WINDOWS
#else
  int sv[2] = {};
  int rc = ::socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
  REQUIRE(rc == 0);


  handler01 handler_instance;
  boson::internal::netpoller<int> loop(handler_instance);
  loop.signal_new_fd(sv[0]);
  loop.register_read(sv[0], 1);
  loop.register_write(sv[0], 2);

  loop.wait(0);
  CHECK(handler_instance.last_read_fd == -1);
  CHECK(handler_instance.last_write_fd == 2);
  CHECK(handler_instance.last_status == 0);

  // Write at the other end, it should work even though we suppressed the other event
  size_t data{1};
  ::send(sv[1],&data, sizeof(size_t),0);
  handler_instance.last_write_fd = -1;
  loop.unregister_write(sv[0]);
  loop.wait();
  CHECK(handler_instance.last_read_fd == 1);
  CHECK(handler_instance.last_write_fd == -1);  // This is unexpected but a write event happens here
  CHECK(handler_instance.last_status == 0);

  handler_instance.last_read_fd = -1;
  handler_instance.last_write_fd = -1;
  loop.wait(0);
  CHECK(handler_instance.last_read_fd == -1);
  CHECK(handler_instance.last_write_fd == -1);
  CHECK(handler_instance.last_status == 0);
  
  loop.signal_fd_closed(sv[0]);
  ::shutdown(sv[0], SHUT_WR);
  ::shutdown(sv[1], SHUT_WR);
  ::close(sv[0]);
  ::close(sv[1]);

  loop.register_read(sv[0], 1);
  loop.register_write(sv[0], 2);
  loop.wait(0);
  CHECK(handler_instance.last_read_fd == 1);
  CHECK(handler_instance.last_write_fd == 2);
  CHECK(handler_instance.last_status == -EBADF);
#endif
}
