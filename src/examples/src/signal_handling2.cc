#include <signal.h>
#include <sys/signalfd.h>
#include <iostream>
#include "boson/select.h"
#include "boson/boson.h"
#include "boson/channel.h"
#include "boson/syscalls.h"

using namespace boson;
using sigfdinfo_t = signalfd_siginfo;
static constexpr size_t nb_threads = 8;

void handle_signals(int signal_fd) {
  sigfdinfo_t info{};
  while(0 < boson::read(signal_fd, &info, sizeof(sigfdinfo_t))) {
    std::cout << "Received signal " << info.ssi_signo << std::endl;
  }
}

int main(int argc, char *argv[]) {
  // Block signals in all threads
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGTERM);
  sigaddset(&mask, SIGINT);
  pthread_sigmask(SIG_BLOCK, &mask, nullptr);
  int signal_fd = signalfd(-1, &mask, SFD_NONBLOCK);

  // Start a boson engine with signal handling
  boson::run(nb_threads, [signal_fd]() {
    start(handle_signals, signal_fd);
    // Do what you mean
  });
}
