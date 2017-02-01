#include <unistd.h>
#include <chrono>
#include <iostream>
#include "boson/boson.h"
#include "boson/logger.h"

using namespace std::literals;

static constexpr int nb_iter = 10;

void producer(int in, int out) {
  int ack_result = 0;
  for (int i = 0; i < nb_iter; ++i) {
    boson::write(out, &i, sizeof(int));
    boson::read(in, &ack_result, sizeof(int));
    if (ack_result == i) boson::debug::log("A: ack succeeded.");
  }
}

void router(int source_in, int source_out, int dest_in, int dest_out) {
  int result = 0;
  while (result < nb_iter - 1) {
    boson::read(source_in, &result, sizeof(int));
    boson::write(dest_out, &result, sizeof(int));
    boson::read(dest_in, &result, sizeof(int));
    boson::write(source_out, &result, sizeof(int));
  }
}

void consumer(int in, int out) {
  int result = 0;
  while (result < nb_iter - 1) {
    boson::read(in, &result, sizeof(int));
    boson::write(out, &result, sizeof(int));
    boson::debug::log("C received: {}", result);
  }
}

void set_no_block(int pipe_fd) {
  auto flags = ::fcntl(pipe_fd, F_GETFD);
  ::fcntl(pipe_fd, F_SETFL, flags | O_NONBLOCK);
}

int main(int argc, char* argv[]) {
  // Set global logger
  boson::debug::logger_instance(&std::cout);

  // Creates some pipes
  int a2b[2];
  ::pipe(a2b);
  int b2a[2];
  ::pipe(b2a);
  int b2c[2];
  ::pipe(b2c);
  int c2b[2];
  ::pipe(c2b);

  // Make non block
  ::set_no_block(a2b[1]);
  ::set_no_block(b2a[0]);
  ::set_no_block(a2b[0]);
  ::set_no_block(b2a[1]);
  ::set_no_block(b2c[1]);
  ::set_no_block(c2b[0]);
  ::set_no_block(b2c[0]);
  ::set_no_block(c2b[1]);

  // Execute a routine communication through pipes
  boson::run(1, [&]() {
    using namespace boson;
    start(producer, b2a[0], a2b[1]);
    start(router, a2b[0], b2a[1], c2b[0], b2c[1]);
    start(consumer, b2c[0], c2b[1]);
  });

  ::close(a2b[1]);
  ::close(b2a[0]);
  ::close(a2b[0]);
  ::close(b2a[1]);
  ::close(b2c[1]);
  ::close(c2b[0]);
  ::close(b2c[0]);
  ::close(c2b[1]);
}
