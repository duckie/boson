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
#include "event_loop_impl.h"

using namespace boson;

namespace {
struct handler01 : public internal::net_event_handler<int> {
  int last_read_fd{-1};
  int last_write_fd{-1};
  event_status last_status {};

  void read(int data, event_status status) override {
    last_read_fd = data;
    last_status = status;
  }
  void write(int data, event_status status) override {
    last_write_fd = data;
    last_status = status;
  }
  void callback() override {};
};
}

/*TEST_CASE("Netpoller - Event notification", "[netpoller][notif]") {
  handler01 handler_instance;

  boson::internal::netpoller<int> loop(handler_instance);
  //int event_id = loop.register_event(nullptr);
//
  //std::thread t1{[&loop]() { loop.loop(1); }};
//
  //loop.send_event(event_id);
  //t1.join();
//
  //CHECK(handler_instance.last_status == 0);
}*/

TEST_CASE("Netpoller - FD Read/Write", "[netpoller][read/write]") {
  handler01 handler_instance;
  int pipe_fds[2];
  ::pipe(pipe_fds);
  ::fcntl(pipe_fds[0], F_SETFL, ::fcntl(pipe_fds[0], F_GETFD) | O_NONBLOCK);
  ::fcntl(pipe_fds[1], F_SETFL, ::fcntl(pipe_fds[1], F_GETFD) | O_NONBLOCK);

  boson::internal::netpoller<int> loop(handler_instance);
  loop.register_read(pipe_fds[0], 1);
  loop.register_write(pipe_fds[1], 2);

  loop.loop(1);
  CHECK(handler_instance.last_write_fd == 2);
  CHECK(handler_instance.last_status == 0);

  size_t data{1};
  ::write(pipe_fds[1], &data, sizeof(size_t));

  loop.loop(1);
  CHECK(handler_instance.last_read_fd == 1);
}
/*
TEST_CASE("Netpoller - FD Read/Write same FD", "[netpoller][read/write]") {
#ifdef WINDOWS
#else
  int sv[2] = {};
  int rc = ::socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
  REQUIRE(rc == 0);


  handler01 handler_instance;
  boson::event_loop loop(handler_instance,1);
  int event_read = loop.register_read(sv[0], nullptr);
  int event_write = loop.register_write(sv[0], nullptr);

  loop.loop(1);
  CHECK(handler_instance.last_read_fd == -1);
  CHECK(handler_instance.last_write_fd == sv[0]);
  CHECK(handler_instance.last_status == 0);

  loop.unregister(event_write);
  // Write at the other end, it should work even though we suppressed the other event
  size_t data{1};
  ::send(sv[1],&data, sizeof(size_t),0);
  handler_instance.last_write_fd = -1;
  loop.loop(1);
  CHECK(handler_instance.last_read_fd == sv[0]);
  CHECK(handler_instance.last_write_fd == -1);
  CHECK(handler_instance.last_status == 0);
  
  ::shutdown(sv[0], SHUT_WR);
  ::shutdown(sv[1], SHUT_WR);
  ::close(sv[0]);
  ::close(sv[1]);
#endif
}

TEST_CASE("Netpoller - FD Panic Read/Write", "[netpoller][panic]") {
  handler01 handler_instance;
  int pipe_fds[2];
  ::pipe(pipe_fds);
  ::fcntl(pipe_fds[0], F_SETFL, ::fcntl(pipe_fds[0], F_GETFD) | O_NONBLOCK);
  ::fcntl(pipe_fds[1], F_SETFL, ::fcntl(pipe_fds[1], F_GETFD) | O_NONBLOCK);

  boson::event_loop loop(handler_instance,1);
  loop.register_read(pipe_fds[0], nullptr);

  loop.loop(1,0);
  CHECK(handler_instance.last_read_fd == -1);

  loop.send_fd_panic(0,pipe_fds[0]);
  loop.loop(1);
  CHECK(handler_instance.last_read_fd == pipe_fds[0]);
  CHECK(handler_instance.last_status < 0);
}

TEST_CASE("Netpoller - Bas fds", "[netpoller]") {
  handler01 handler_instance;
  std::string temp1 = std::tmpnam(nullptr);
  int disk_fd = ::open(temp1.c_str(), O_RDWR | O_CREAT | O_NONBLOCK, 0600);

  boson::event_loop loop(handler_instance,1);
  try {
    loop.register_read(disk_fd, nullptr);
  }
  catch(boson::exception& e) {
    CHECK(std::strncmp(e.what(),"Syscall error (epoll_ctl): Operation not permitted",26) == 0);
    CHECK(true);
  }

  ::close(disk_fd);
  ::unlink(temp1.c_str());
}
*/
