#ifndef BOSON_NETPOLLER_H_
#define BOSON_NETPOLLER_H_

#include "../io_event_loop.h"
#include "../queues/mpsc.h"
#include "../utility.h"
#include "thread.h"
#include <chrono>

namespace boson {
namespace internal {

struct netpoller_platform_impl {
  std::unique_ptr<io_event_loop> loop_;
  
  netpoller_platform_impl(io_event_handler& handler);
  ~netpoller_platform_impl();
  void register_fd(fd_t fd);
  void unregister(fd_t fd);
  io_loop_end_reason loop(int nb_iter, int timeout_ms);
  void interrupt();

  static size_t get_max_fds();
};

template <class Data> struct net_event_handler {
  virtual void read(fd_t fd, Data data, event_status status) = 0;
  virtual void write(fd_t fd, Data data, event_status status) = 0;
  virtual void callback() = 0;
};

/** 
 * Implements the event_loop from boson perspective
 *
 * The io_event_loop is a simple loop made to contain platform specific
 * asyncrhonous event code.
 *
 * The netpoller extends said loop to implements boson's behavior and policy
 * regarding fd management.
 */
template <class Data> 
class netpoller : public io_event_handler, private netpoller_platform_impl {
  net_event_handler<Data>& handler_;

  struct fd_data {
    std::mutex read_lock; // TODO Replace byt bitmasked atomic data
    std::mutex write_lock; // TODO Replace byt bitmasked atomic data
    bool read_missed;
    bool write_missed;
    bool read_enabled;
    bool write_enabled;
    Data read_data;
    Data write_data;
  };

  /**
   * For this implementaiton, we consider open FDs to be dense
   *
   * That would not be the case on Windows where a map of some
   * kind must be used
   */
  std::vector<fd_data> waiters_;

  void dispatchRead(fd_t fd, event_status status) {
    auto& current_data = waiters_[fd];
    std::lock_guard<std::mutex> read_guard(current_data.read_lock);
    if (current_data.read_enabled) {
      handler_.read(fd, current_data.read_data, status);
      current_data.read_enabled = false;
    }
    else
      current_data.read_missed = true;
  }

  void dispatchWrite(fd_t fd, event_status status) {
    auto& current_data = waiters_[fd];
    std::lock_guard<std::mutex> write_guard(current_data.write_lock);
    if (current_data.write_enabled) {
      handler_.write(fd, current_data.write_data, status);
      current_data.write_enabled = false;
    }
    else
      current_data.write_missed = true;
  }

 public:
  netpoller(net_event_handler<Data>& handler)
      : netpoller_platform_impl{static_cast<io_event_handler&>(*this)},
        handler_{handler},
        waiters_(netpoller_platform_impl::get_max_fds()) {
  }

  void read(fd_t fd, event_status status) override {
    dispatchRead(fd, status);
  }

  void write(fd_t fd, event_status status) override {
    dispatchWrite(fd, status);
  }

  void closed(fd_t fd) override {
    dispatchRead(fd, -EBADF);
    dispatchWrite(fd, -EBADF);
  }

  void interrupt() {
    netpoller_platform_impl::interrupt();
  }

  /**
   * Tells the net poller a new fd should be watched
   *
   * Can be called from any thread
   */
  void signal_new_fd(fd_t fd) {
    netpoller_platform_impl::register_fd(fd);
    { 
      std::lock_guard<std::mutex> read_guard(waiters_[fd].read_lock);
      waiters_[fd].read_missed = false;
      waiters_[fd].read_enabled = false;
      waiters_[fd].read_data = -1;
    }
    {
      std::lock_guard<std::mutex> write_guard(waiters_[fd].write_lock);
      waiters_[fd].write_missed = false;
      waiters_[fd].write_enabled = false;
      waiters_[fd].write_data = -1;
    }
  }

  /**
   * Tells the netpoller the fd will not produce events anymore
   *
   * Can be called from any thread
   */
  void signal_fd_closed(fd_t fd) {
    netpoller_platform_impl::unregister(fd);
    netpoller_platform_impl::interrupt();
  }

  /**
   * Register for a read callback
   *
   * Can be called from any thread
   */
  void register_read(fd_t fd, Data value) {
    assert(0 <= fd);
    std::lock_guard<std::mutex> read_guard(waiters_[fd].read_lock);
    waiters_[fd].read_data = value;
    waiters_[fd].read_enabled = true;
    if (waiters_[fd].read_missed) {
      waiters_[fd].read_missed = false;
      handler_.read( fd, value, 0);  // TODO: handle error here
    }
  }

  /**
   * Register for a write callback
   *
   * Can be called from any thread
   */
  void register_write(fd_t fd, Data value) {
    assert(0 <= fd);
    std::lock_guard<std::mutex> write_guard(waiters_[fd].write_lock);
    waiters_[fd].write_data = value;
    waiters_[fd].write_enabled = true;
    if (waiters_[fd].write_missed) {
      waiters_[fd].write_missed = false;
      handler_.write( fd, value, 0);  // TODO: handle error here
    }
  }

  /**
   * Unregister for read
   *
   * Can be called from any thread
   */
  void unregister_read(fd_t fd) {
    assert(0 <= fd);
    std::lock_guard<std::mutex> read_guard(waiters_[fd].read_lock);
    waiters_[fd].read_enabled = false;
    waiters_[fd].read_data = -1;
  }

  /**
   * Unregister for write
   *
   * Can be called from any thread
   */
  void unregister_write(fd_t fd) {
    assert(0 <= fd);
    std::lock_guard<std::mutex> write_guard(waiters_[fd].write_lock);
    waiters_[fd].write_enabled = false;
    waiters_[fd].write_data = -1;
  }

  /**
   * Loops onto events 
   *
   * The timeout is repeated for every iteration. With nb_iter < 0, it 
   * can be used as a kind of GC to be sure to unqueue requests
   */
  io_loop_end_reason loop(int nb_iter = -1, int timeout_ms = -1) {
    int current_iter = 0;
    while (current_iter < nb_iter || nb_iter < 0) {
      // Loop once
      auto end_reason = netpoller_platform_impl::loop(nb_iter, timeout_ms);

      // Tells the handler we looped
      handler_.callback();

      switch (end_reason) {
        case io_loop_end_reason::max_iter_reached:
          break;
        case io_loop_end_reason::timed_out:
        case io_loop_end_reason::error_occured:
          return end_reason;
      }

      ++current_iter;
    }

    return io_loop_end_reason::max_iter_reached;
  }

  bool get_read_data(fd_t fd, Data& data) {
    std::lock_guard<std::mutex> read_guard(waiters_[fd].read_lock);
    data = waiters_[fd].read_data;
    return waiters_[fd].read_enabled;
  }

  bool get_write_data(fd_t fd, Data& data) {
    std::lock_guard<std::mutex> write_guard(waiters_[fd].write_lock);
    data = waiters_[fd].write_data;
    return waiters_[fd].write_enabled;
  }

  template <class Duration>
  io_loop_end_reason loop(int nb_iter, Duration&& duration) {
    return this->loop(
        nb_iter, std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
  }
};

}
}

#endif  // BOSON_NETPOLLER_H_
