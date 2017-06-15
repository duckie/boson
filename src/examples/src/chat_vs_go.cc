#include "boson/boson.h"
#include "boson/logger.h"
#include "boson/channel.h"
#include "boson/net/socket.h"
#include "boson/select.h"
#include "boson/shared_buffer.h"
#include "fmt/format.h"
#include "iostream"
#include <atomic>
#include <chrono>
#include <fcntl.h>
#include <iostream>
#include <netinet/tcp.h>
#include <random>
#include <set>
#include <signal.h>
#include <string>

// sudo perf stat -e 'syscalls:sys_enter_epoll*'

using namespace boson;
using namespace std::chrono_literals;

void listen_client(int fd, channel<std::string, 5> msg_chan) {
  std::array<char, 2048> buffer;
  for (;;) {
    ssize_t nread = boson::read(fd, buffer.data(), buffer.size());
    if (nread <= 1) {
      boson::close(fd);
      std::exit(1);
      return;
    }
    std::string message(buffer.data(), nread);
    if (message == "quit") {
      boson::close(fd);
      return;
    }
    msg_chan << message;
  }
}

void handleNewConnections(boson::channel<int, 5> newConnChan) {
  int yes = 1;
  int sockfd = net::create_listening_socket(8080);
  struct sockaddr_in cli_addr;
  socklen_t clilen = sizeof(cli_addr);
  for (;;) {
    int newFd = boson::accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
    if (0 <= newFd) {
      if (::setsockopt(newFd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) < 0)
        throw boson::exception("setsockopt");
      newConnChan << newFd;
    }
  }
}

void displayCounter(std::atomic<uint64_t>* counter) {
  using namespace std::chrono;
  auto latest = high_resolution_clock::now();
  milliseconds step_duration {1000};
  //for (int i = 0;i < 60; ++i) {
  for (;;) {
    ::printf("%lu\n",counter->exchange(0,std::memory_order_relaxed)*1000/step_duration.count());
    boson::sleep(1000ms);
    auto current = high_resolution_clock::now();
    step_duration = duration_cast<milliseconds>(current - latest);
    latest = current;
  }
  std::exit(0);
}

int main(int argc, char *argv[]) {
  boson::debug::logger_instance(&std::cout);
  struct sigaction action {SIG_IGN,0,0};
  ::sigaction(SIGPIPE, &action, nullptr);
  boson::run(1, []() {
    channel<int, 5> new_connection;
    channel<std::string, 5> messages;
    std::atomic<uint64_t> counter {0};
    boson::start(displayCounter, &counter);
    boson::start(handleNewConnections, new_connection);

    // Create socket and list to connections

    // Main loop
    //std::set<int> conns;
    size_t cumulatedCounter = 0;
    std::vector<int> conns;
    std::minstd_rand rand(std::random_device{}());
    while(cumulatedCounter < 1e6) {
      int conn = 0;
      int scheduler = 0;
      std::string message;
      select_any(                           //
          event_read(new_connection, conn,  //
                     [&](bool) {            //
                       conns.push_back(conn);
                       //start_explicit(++scheduler % 3 + 1, listen_client, conn, messages);
                       start(listen_client, conn, messages);
                     }),
          event_read(messages, message,
                     [&](bool result) {  //
                       if (result) {
                         message += "\n";
                         std::uniform_int_distribution<size_t> dist(0, conns.size() - 1);
                         ssize_t rc = boson::write(conns[dist(rand)], message.data(), message.size());
                         if (rc < 0)
                          std::exit(1);
                         auto value = counter.fetch_add(1, std::memory_order_relaxed);
                         ++cumulatedCounter;
                       }
                     }));
    };
    std::exit(0);
  });
}
