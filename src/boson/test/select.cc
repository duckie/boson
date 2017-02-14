#include "catch.hpp"
#include "boson/boson.h"
#include <unistd.h>
#include <iostream>
#include "boson/logger.h"
#include "boson/semaphore.h"
#include "boson/select.h"
#include "boson/net/socket.h"
#ifdef BOSON_USE_VALGRIND
#include "valgrind/valgrind.h"
#endif 

using namespace boson;
using namespace std::literals;

namespace {
inline int time_factor() {
#ifdef BOSON_USE_VALGRIND
  return RUNNING_ON_VALGRIND ? 10 : 1;
#else
  return 1;
#endif 
}
}

TEST_CASE("Routines - I/O", "[routines][i/o]") {
  boson::debug::logger_instance(&std::cout);


  SECTION("Simple pipes") {
    int pipe_fds[2];
    ::pipe(pipe_fds);
    ::fcntl(pipe_fds[0], F_SETFL, ::fcntl(pipe_fds[0], F_GETFD) | O_NONBLOCK);
    ::fcntl(pipe_fds[1], F_SETFL, ::fcntl(pipe_fds[1], F_GETFD) | O_NONBLOCK);

    boson::run(1, [&]() {
      start(
          [](int in) -> void {
            size_t data;
            int result = boson::read(in, &data, sizeof(size_t), time_factor()*5ms);
            CHECK(result == -1);
            CHECK(errno == ETIMEDOUT);
            result = boson::read(in, &data, sizeof(size_t));
            CHECK(0 < result);
          },
          pipe_fds[0]);

      start(
          [](int out) -> void {
            size_t data{0};
            boson::sleep(time_factor()*10ms);
            int result = boson::write(out, &data, sizeof(data));
            CHECK(0 < result);
          },
          pipe_fds[1]);
    });

    ::close(pipe_fds[0]);
    ::close(pipe_fds[1]);
  }
}

TEST_CASE("Routines - Select", "[routines][i/o][select]") {
  boson::debug::logger_instance(&std::cout);

  SECTION("Simple pipes - reads") {
    int pipe_fds1[2];
    ::pipe(pipe_fds1);
    ::fcntl(pipe_fds1[0], F_SETFL, ::fcntl(pipe_fds1[0], F_GETFD) | O_NONBLOCK);
    ::fcntl(pipe_fds1[1], F_SETFL, ::fcntl(pipe_fds1[1], F_GETFD) | O_NONBLOCK);
    int pipe_fds2[2];
    ::pipe(pipe_fds2);
    ::fcntl(pipe_fds2[0], F_SETFL, ::fcntl(pipe_fds2[0], F_GETFD) | O_NONBLOCK);
    ::fcntl(pipe_fds2[1], F_SETFL, ::fcntl(pipe_fds2[1], F_GETFD) | O_NONBLOCK);

    boson::run(1, [&]() {
      boson::channel<std::nullptr_t,1> tickets;
      start(
          [](int in1, int in2, auto tickets) -> void {
            int result = 0;
            size_t data;
            result = boson::select_any(                 //
                event_read(in1, &data, sizeof(size_t),  //
                           [](ssize_t) {
                             return 1;  //
                           }),
                event_read(in2, &data, sizeof(size_t),  //
                           [](ssize_t) {
                             return 2;  //
                           }));
            CHECK(result == 2);
            tickets << nullptr;
            result = boson::select_any(                 //
                event_read(in1, &data, sizeof(size_t),  //
                           [](ssize_t) {
                             return 1;  //
                           }),
                event_read(in2, &data, sizeof(size_t),  //
                           [](ssize_t) {
                             return 2;  //
                           }));
            CHECK(result == 1);
          },
          pipe_fds1[0], pipe_fds2[0], tickets);

      start(
          [](int out1, int out2, auto tickets) -> void {
            std::nullptr_t sink;
            size_t data{0};
            boson::write(out2, &data, sizeof(data));
            tickets >> sink; // Wait for other routin to finish first test
            boson::write(out1, &data, sizeof(data));
          },
          pipe_fds1[1], pipe_fds2[1], tickets);
    });

    ::close(pipe_fds1[0]);
    ::close(pipe_fds1[1]);
    ::close(pipe_fds2[0]);
    ::close(pipe_fds2[1]);
  }

  SECTION("Simple pipes - writes") {
    int pipe_fds1[2];
    ::pipe(pipe_fds1);
    ::fcntl(pipe_fds1[0], F_SETFL, ::fcntl(pipe_fds1[0], F_GETFD) | O_NONBLOCK);
    ::fcntl(pipe_fds1[1], F_SETFL, ::fcntl(pipe_fds1[1], F_GETFD) | O_NONBLOCK);
    int pipe_fds2[2];
    ::pipe(pipe_fds2);
    ::fcntl(pipe_fds2[0], F_SETFL, ::fcntl(pipe_fds2[0], F_GETFD) | O_NONBLOCK);
    ::fcntl(pipe_fds2[1], F_SETFL, ::fcntl(pipe_fds2[1], F_GETFD) | O_NONBLOCK);

    boson::run(1, [&]() {
      start(
          [](int in1, int in2) -> void {
            size_t data{0};
            boson::read(in1, &data, sizeof(size_t));
            boson::read(in2, &data, sizeof(size_t));
          },
          pipe_fds1[0], pipe_fds2[0]);

      start(
          [](int in1, int int2, int out1, int out2) -> void {
            size_t data{0};
            int result = boson::select_any(                 //
                event_write(out1, &data, sizeof(size_t),  //
                           [](ssize_t) {
                             return 1;  //
                           }),
                event_write(out2, &data, sizeof(size_t),  //
                           [](ssize_t) {
                             return 2;  //
                           }));
            CHECK(result == 1);
            
            // Fill up the pipe to make next try blocking
            ssize_t pipe_size = ::fcntl(out1, F_GETPIPE_SZ);
            for(ssize_t index = 0; index < (pipe_size - sizeof(size_t)); ++index) {
              boson::write(out1,"",1);
            }

            result = boson::select_any(                 //
                event_write(out1, &data, sizeof(size_t),  //
                           [](ssize_t rc) {
                             return 1;  //
                           }),
                event_write(out2, &data, sizeof(size_t),  //
                           [](ssize_t) {
                             return 2;  //
                           }));
            CHECK(result == 2);
          },
          pipe_fds1[0], pipe_fds2[0], pipe_fds1[1], pipe_fds2[1]);
    });

    ::close(pipe_fds1[0]);
    ::close(pipe_fds1[1]);
    ::close(pipe_fds2[0]);
    ::close(pipe_fds2[1]);
  }

  std::this_thread::sleep_for(time_factor()*10ms);
  //SECTION("Channels") {
  SECTION("Channels") {
    boson::run(1, [&]() {
      boson::channel<int,1> tickets_a2b;
      boson::channel<int,1> tickets_b2a;
      boson::channel<int,1> tickets1;
      boson::channel<int,1> tickets2;
      start(
          [](auto t1, auto t2, auto a2b, auto b2a) -> void {
            int sink;
            int result = 0;
            int chandata;
            result = boson::select_any(                //
                event_read(t1, chandata, //
                           [](bool) {
                             return 1;  //
                           }),
                event_read(t2, chandata,  //
                           [](bool) {
                             return 2;  //
                           }),
                event_timer(0, //
                           []() {
                             return 3;  //
                           }));
            CHECK(result == 3);
            a2b << 0;  // Allow producer to continue

            result = boson::select_any(                //
                event_read(t1, chandata, //
                           [](bool) {
                             return 1;  //
                           }),
                event_read(t2, chandata,  //
                           [](bool) {
                             return 2;  //
                           }));
            CHECK(result == 2);
            a2b << 0;

            // My turn to wait a ticket
            b2a >> sink;

            t1 >> chandata;
            CHECK(chandata == 2);
            t2 >> chandata;
            CHECK(chandata == 3);
          },
          tickets1, tickets2, tickets_a2b, tickets_b2a);

      start(
          [](auto t1, auto t2, auto a2b, auto b2a) -> void {
            //std::nullptr_t sink;
            int sink;
            a2b >> sink;
            int result = -1;

            t2 << 1;
            a2b >> sink; // Wait for other routine to consume

            result = boson::select_any(                //
                event_write(t1, 2, //
                           [](bool) {
                             return 1;  //
                           }),
                event_write(t2, 3,  //
                           [](bool) {
                             return 2;  //
                           }));
            CHECK(result == 1);

            result = boson::select_any(                //
                event_write(t1, 2, //
                           [](bool) {
                             return 1;  //
                           }),
                event_write(t2, 3,  //
                           [](bool) {
                             return 2;  //
                           }));
            CHECK(result == 2);

            b2a << 0;
          },
          tickets1, tickets2, tickets_a2b, tickets_b2a);
    });
  }

  SECTION("Pipe and channels") {
    int pipe_fds1[2];
    ::pipe(pipe_fds1);
    ::fcntl(pipe_fds1[0], F_SETFL, ::fcntl(pipe_fds1[0], F_GETFD) | O_NONBLOCK);
    ::fcntl(pipe_fds1[1], F_SETFL, ::fcntl(pipe_fds1[1], F_GETFD) | O_NONBLOCK);

    boson::run(1, [&]() {
      boson::channel<std::nullptr_t,5> tickets;
      boson::channel<int,5> pipe2;
      pipe2 << 1;
      start(
          [](int in1, auto in2, auto tickets) -> void {
            int result = 0;
            size_t data;
            int chandata;
            result = boson::select_any(                 //
                event_read(in1, &data, sizeof(size_t),  //
                           [](ssize_t) {
                             return 1;  //
                           }),
                event_read(in2, chandata,  //
                           [](bool) {
                             return 2;  //
                           }));
            CHECK(result == 2);
            result = boson::select_any(                 //
                event_read(in1, &data, sizeof(size_t),  //
                           [](ssize_t) {
                             return 1;  //
                           }),
                event_read(in2, chandata,  //
                           [](bool) {
                             return 2;  //
                           }));
            CHECK(result == 2);
            tickets << nullptr;
            result = boson::select_any(                 //
                event_read(in1, &data, sizeof(size_t),  //
                           [](ssize_t) {
                             return 1;  //
                           }),
                event_read(in2, chandata,  //
                           [](bool) {
                             return 2;  //
                           }));
            CHECK(result == 1);
          },
          pipe_fds1[0], pipe2, tickets);

      start(
          [](int out1, auto out2, auto tickets) -> void {
            std::nullptr_t sink;
            size_t data{0};
            out2 << 1;
            tickets >> sink; // Wait for other routin to finish first test
            boson::write(out1, &data, sizeof(data));
          },
          pipe_fds1[1], pipe2, tickets);
    });

    ::close(pipe_fds1[0]);
    ::close(pipe_fds1[1]);
  }

  SECTION("Closing channels") {
    boson::run(1, [&]() {
      boson::channel<std::nullptr_t, 1> chan1;
      boson::channel<std::nullptr_t, 1> chan2;

      start(
          [](auto c1, auto c2) -> void {
            std::nullptr_t value{};
            // c2.read(value);
            int result = boson::select_any(  //
                event_read(c1, value,        //
                           [](bool success) { return success ? 1 : 2; }),
                event_read(c2, value,  //
                           [](bool success) { return success ? 3 : 4; }));
            CHECK(result == 4);

            // Re test, a closed channel should always drop an event immediately
            result = boson::select_any(  //
                event_read(c1, value,    //
                           [](bool success) { return success ? 1 : 2; }),
                event_read(c2, value,  //
                           [](bool success) { return success ? 3 : 4; }));
            CHECK(result == 4);

            // Test with write close
            c1 << nullptr;
            result = boson::select_any(   //
                event_write(c1, nullptr,  //
                            [](bool success) { return success ? 1 : 2; }));
            CHECK(result == 2);
          },
          chan1, chan2);

      start(
          [](auto c1, auto c2) -> void {
            c2.close();
          },
          chan1, chan2);

      start(
          [](auto c1, auto c2) -> void {
            std::nullptr_t value;
            // Wait ticket
            c1 >> value;
            // Close chanel
            c1.close();
          },
          chan1, chan2);
    });
  }

  SECTION("Select on mutex") {
    boson::run(1, [&]() {
      boson::mutex mut1, mut2;
      mut1.lock();
      mut2.lock();

      start([](auto m1, auto m2) -> void {
        using namespace boson;
        int result = select_any(                 //
            event_lock(m1, []() { return 1; }),  //
            event_lock(m2, []() { return 2; })   //
            );
        CHECK(result == 2);
        result = select_any(                 //
            event_lock(m1, []() { return 1; }),  //
            event_lock(m2, []() { return 2; })   //
            );
        CHECK(result == 1);
      }, mut1, mut2);

      start([](auto m1, auto m2) -> void {
        m2.unlock();
        m1.unlock();
      }, mut1, mut2);
    });
  }

  SECTION("Select on accept/connect") {
    // A routine that connects to itself in a single thread
    boson::run(1, [&]() {
      using namespace boson;

      // Listening socker
      int listening_socket = boson::net::create_listening_socket(10101);
      struct sockaddr_in cli_addr;
      socklen_t clilen = sizeof(cli_addr);

      // Connecting socket
      struct sockaddr_in cli_addr2;
      cli_addr2.sin_addr.s_addr = ::inet_addr("127.0.0.1");
      cli_addr2.sin_family = AF_INET;
      cli_addr2.sin_port = htons(10101);
      socklen_t clilen2 = sizeof(sockaddr);
      int sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
      ::fcntl(sockfd, F_SETFL, O_NONBLOCK);
      
      // Accept/connect at the same time
      int result = select_any( //
        event_accept(listening_socket, (struct sockaddr*)&cli_addr, &clilen,[](int rc) {
          CHECK(0 < rc);
          return 0;
          }), //
        event_connect(sockfd, (struct sockaddr*)&cli_addr2, sizeof(struct sockaddr), [](int rc) {
            return 1;
          }) //
        ); 
      CHECK(result == 0);

      result = select_any( //
        event_accept(listening_socket, (struct sockaddr*)&cli_addr, &clilen,[](int) {
          return 0;
          }), //
        event_connect(sockfd, (struct sockaddr*)&cli_addr2, clilen2, [&sockfd](int rc) {
            CHECK(rc == 0);
            ::shutdown(sockfd, SHUT_WR);
            ::close(sockfd);
            return 1;
          }) //
        ); 
      CHECK(result == 1);

      ::close(listening_socket);
    });
  }
}
