#include <signal.h>
#include <sys/signalfd.h>
#include <iostream>
#include "boson/select.h"
#include "boson/boson.h"
#include "boson/channel.h"
#include "boson/syscalls.h"

using namespace boson;
using sigfdinfo_t = signalfd_siginfo;
static constexpr size_t nb_threads = 2;

void handle_signals(int signal_fd, channel<bool,1> stopper_in, channel<bool,10> stopper_out) {
  sigfdinfo_t info{};
  bool stop = false;

  while (!stop) {
    select_any( //
               //event_read(stopper_out, stop, [](bool) {
                  //std::cout << "So 3 ? " << std::endl;
               //}),// Stop is affected directly
        event_read(signal_fd, &info, sizeof(sigfdinfo_t),
                          [&](ssize_t rc) {  //
                            std::cout << "Received signal " << info.ssi_signo << "  in thread "
                                      << boson::internal::current_thread()->id() << std::endl;

                            // If SIGTERM, interrupt all signal handlers except me
                            if (rc <= 0 || info.ssi_signo == SIGTERM) {
                              //stopper_in << true;
                              //std::cout << "Yaye" << std::endl;
                            }
                          })
               ); 
  }
}

int main(int argc, char *argv[]) {
  // Block signals in all threads
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGTERM);
  sigaddset(&mask, SIGINT);
  pthread_sigmask(SIG_BLOCK, &mask, nullptr);
  int signal_fd = signalfd(-1, &mask, 0);


  std::cout << signal_fd << std::endl;

  // Start a boson engin with signal handling
  boson::run(nb_threads, [signal_fd]() {
    // Start all signal handlers
    channel<bool,1> stopper_in;
    channel<bool,10> stopper_out;
    // Start channel forwarder
    boson::start([stopper_in,stopper_out]() mutable {
        bool value {};
        stopper_in >> value; // Wait for the fire
          std::cout << "So 1?" << std::endl;
        for (int t = 0; t < nb_threads; ++t) {
          std::cout << "So 2 ?" << std::endl;
          stopper_out << true;
        }
    });
    // Start signal handlers
    for (int t = 0; t < nb_threads; ++t) {
      start_explicit(t, handle_signals, signal_fd, stopper_in, stopper_out);
    }
    stopper_in << true;

    // Do what you mean
  });
}
