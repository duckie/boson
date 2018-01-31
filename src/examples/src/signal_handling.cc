#include <signal.h>
#include <sys/signalfd.h>
#include <iostream>
#include "boson/boson.h"
#include "boson/channel.h"
#include "boson/select.h"
#include "boson/syscalls.h"

using namespace boson;
using sigfdinfo_t = signalfd_siginfo;
static constexpr size_t nb_threads = 8;

void handle_signals(int signal_fd, channel<bool, 1> stopper) {
  sigfdinfo_t info{};
  bool stop = false;
  while (!stop) {
    select_any(  //
        event_read(stopper, stop, [&stop](bool) { stop = true; }),
        event_read(signal_fd, &info, sizeof(sigfdinfo_t),
                   [&](ssize_t rc) {  //
                     std::cout << "Received signal " << info.ssi_signo << "  in thread "
                               << boson::internal::current_thread()->id() << std::endl;

                     // Ignore rc <= 0
                     if (info.ssi_signo == SIGTERM) {
                       stopper.close();
                     }
                   }));
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

  // Start a boson engin with signal handling
  boson::run(nb_threads, [signal_fd]() {
    // Start all signal handlers
    channel<bool, 1> stopper;
    // Start signal handlers in each thread
    for (int t = 0; t < nb_threads; ++t) {
      start_explicit(t, handle_signals, signal_fd, stopper);
    }
    // Do what you mean
  });
}
