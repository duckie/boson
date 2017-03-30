#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <thread>
#include <cstring>
#include "boson/exception.h"
#include "boson/system.h"
#include "boson/logger.h"
#include "catch.hpp"
#include "io_event_loop_impl.h"
#include <iostream>

using namespace boson;

namespace {
struct handler01 : public io_event_handler {
  int last_id{-1};
  int last_read_fd{-1};
  int last_write_fd{-1};
  event_status last_status {};

  void read(fd_t fd, event_status status) override {
    last_id = -1;
    last_read_fd = fd;
    last_status = status;
  }
  void write(fd_t fd, event_status status) override {
    last_id = -1;
    last_write_fd = fd;
    last_status = status;
  }

  void closed(fd_t fd) override {
    last_id = -1;
    last_read_fd = fd;
    last_write_fd = fd;
    last_status = EBADF;
  }
};
}

TEST_CASE("IO Event Loop - Event notification", "[ioeventloop][notif]") {
  boson::debug::logger_instance(&std::cout);
  handler01 handler_instance;
  boson::io_event_loop loop(handler_instance,1);
  // Two times to be sure the eventfd does not trigger write
  // events
  for (int index=0; index < 2; ++index) {
    std::thread t1{[&loop]() { 
      loop.loop(1);
    }};
    loop.interrupt();
    t1.join();
    CHECK(handler_instance.last_id == -1);
    CHECK(handler_instance.last_read_fd == -1);
    CHECK(handler_instance.last_write_fd == -1);
    CHECK(handler_instance.last_status == 0);
  }
}

TEST_CASE("IO Event Loop - FD Read/Write", "[ioeventloop][read/write]") {
  handler01 handler_instance;
  int pipe_fds[2];
  ::pipe(pipe_fds);
  ::fcntl(pipe_fds[0], F_SETFL, ::fcntl(pipe_fds[0], F_GETFD) | O_NONBLOCK);
  ::fcntl(pipe_fds[1], F_SETFL, ::fcntl(pipe_fds[1], F_GETFD) | O_NONBLOCK);

  boson::io_event_loop loop(handler_instance,1);

  std::thread t1{[&loop]() { 
    loop.loop(1);
  }};

  loop.register_fd(pipe_fds[0]);
  loop.register_fd(pipe_fds[1]);
  t1.join();

  CHECK(handler_instance.last_write_fd == pipe_fds[1]);
  CHECK(handler_instance.last_status == 0);

  std::thread t2{[&loop]() { 
    loop.loop(1);
  }};
  size_t data{1};
  ::write(pipe_fds[1], &data, sizeof(size_t));
  t2.join();

  CHECK(handler_instance.last_read_fd == pipe_fds[0]);
}

TEST_CASE("IO Event Loop - FD Read/Write same FD", "[ioeventloop][read/write]") {
#ifdef WINDOWS
#else
  int sv[2] = {};
  int rc = ::socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
  REQUIRE(rc == 0);


  handler01 handler_instance;
  boson::io_event_loop loop(handler_instance,1);
  loop.register_fd(sv[0]);

  loop.loop(1);
  CHECK(handler_instance.last_read_fd == -1);
  CHECK(handler_instance.last_write_fd == sv[0]);
  CHECK(handler_instance.last_status == 0);

  //loop.unregister(sv[0]);
  // Write at the other end, it should work even though we suppressed the other event
  size_t data{1};
  ::send(sv[1],&data, sizeof(size_t),0);
  handler_instance.last_write_fd = -1;
  loop.loop(1);
  CHECK(handler_instance.last_read_fd == sv[0]);
  CHECK(handler_instance.last_write_fd == sv[0]);
  CHECK(handler_instance.last_status == 0);
  
  ::shutdown(sv[0], SHUT_WR);
  ::shutdown(sv[1], SHUT_WR);
  ::close(sv[0]);
  ::close(sv[1]);
#endif
}

TEST_CASE("IO Event Loop - FD Panic Read/Write", "[ioeventloop][panic]") {
  handler01 handler_instance;
  int pipe_fds[2];
  ::pipe(pipe_fds);
  ::fcntl(pipe_fds[0], F_SETFL, ::fcntl(pipe_fds[0], F_GETFD) | O_NONBLOCK);
  ::fcntl(pipe_fds[1], F_SETFL, ::fcntl(pipe_fds[1], F_GETFD) | O_NONBLOCK);

  boson::io_event_loop loop(handler_instance,1);
  loop.register_fd(pipe_fds[0]);

  loop.loop(1,0);
  CHECK(handler_instance.last_read_fd == -1);

  loop.unregister(pipe_fds[0]);
  loop.interrupt();  // Its is unregister caller responsibility to call "interrupt"
  loop.loop(1);
  CHECK(handler_instance.last_read_fd == pipe_fds[0]);
  CHECK(handler_instance.last_write_fd == pipe_fds[0]);
  CHECK(handler_instance.last_status == EBADF);
}

TEST_CASE("IO Event Loop - Bas fds", "[ioeventloop]") {
  //handler01 handler_instance;
  //std::string temp1 = std::tmpnam(nullptr);
  //int disk_fd = ::open(temp1.c_str(), O_RDWR | O_CREAT | O_NONBLOCK, 0600);
//
  //boson::event_loop loop(handler_instance,1);
  //try {
    //loop.register_read(disk_fd, nullptr);
  //}
  //catch(boson::exception& e) {
    //CHECK(std::strncmp(e.what(),"Syscall error (epoll_ctl): Operation not permitted",26) == 0);
    //CHECK(true);
  //}
//
  //::close(disk_fd);
  //::unlink(temp1.c_str());
}
