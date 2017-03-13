#ifndef BOSON_NETPOLLER_H_
#define BOSON_NETPOLLER_H_

#include "../io_event_loop.h"
#include "../event_loop.h"
#include "../queues/mpsc.h"
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
};

template <class Data> struct net_event_handler {
  virtual void read(Data data, event_status status) = 0;
  virtual void write(Data data, event_status status) = 0;
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
  std::atomic<size_t> nb_idle_threads;

  struct fd_data {
    bool read_enabled;
    bool write_enabled;
    Data read_data;
    Data write_data;
  };

  enum class command_type {
    new_fd,
    close_fd,
    update_read,
    update_write,
    remove_read,
    remove_write
  };

  struct command {
    command_type type;
    fd_t fd;
    Data data;
  };

  /**
   * For this implementaiton, we consider open FDs to be dense
   *
   * That would not be the case on Windows where a map of some
   * kind must be used
   */
  std::vector<fd_data> waiters_;
  std::vector<std::pair<fd_t, event_status>> read_events_;
  std::vector<std::pair<fd_t, event_status>> write_events_;
  queues::mpsc<command> pending_updates_;

 public:
  netpoller(net_event_handler<Data>& handler) : netpoller_platform_impl{static_cast<io_event_handler&>(*this)}, handler_{handler} {}

  void read(fd_t fd, event_status status) override {
    read_events_.emplace_back(fd, status);
  }

  void write(fd_t fd, event_status status) override {
    write_events_.emplace_back(fd, status);
  }

  void closed(fd_t fd) override {
    read_events_.emplace_back(fd, -EBADF);
    write_events_.emplace_back(fd, -EBADF);
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
    pending_updates_.write(command { command_type::new_fd, fd, Data{} });
    netpoller_platform_impl::register_fd(fd);
  }

  /**
   * Tells the netpoller the fd will not produce events anymore
   *
   * Can be called from any thread
   */
  void signal_fd_closed(fd_t fd) {
    pending_updates_.write(command { command_type::close_fd, fd, Data{} });
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
    pending_updates_.write(command { command_type::update_read, fd, value });
  }

  /**
   * Register for a write callback
   *
   * Can be called from any thread
   */
  void register_write(fd_t fd, Data value) {
    assert(0 <= fd);
    pending_updates_.write(command { command_type::update_write, fd, value });
  }

  /**
   * Unregister for read
   *
   * Can be called from any thread
   */
  void unregister_read(fd_t fd) {
    assert(0 <= fd);
    pending_updates_.write(command { command_type::remove_read, fd, Data{} });
  }

  /**
   * Unregister for write
   *
   * Can be called from any thread
   */
  void unregister_write(fd_t fd) {
    assert(0 <= fd);
    pending_updates_.write(command { command_type::remove_write, fd, Data{} });
  }

  /**
   * Unregister for read and returns stored data
   *
   * Can only be called from the same thread than loop()
   */
  Data local_unregister_read(fd_t fd) {
    assert(0 <= fd && fd < waiters_.size());
    unregister_read(fd);
    return waiters_[fd].read_data;
  }

  /**
   * Unregister for write and returns stored data
   *
   * Can only be called from the same thread than loop()
   */
  Data local_unregister_write(fd_t fd) {
    assert(0 <= fd && fd < waiters_.size());
    unregister_write(fd);
    return waiters_[fd].write_data;
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

      // Unqueue the commands
      command current_command;
      while (pending_updates_.read(current_command)) {
        size_t index = static_cast<size_t>(current_command.fd);
        switch (current_command.type) {
          case command_type::new_fd: {
            if (waiters_.size() <= index) {
              waiters_.resize(index + 1);
            }
            waiters_[index].read_enabled = false;
            waiters_[index].write_enabled = false;
          } break;
          case command_type::close_fd: {
            assert(index < waiters_.size());                                         
            if (waiters_[index].read_enabled)
              read_events_.emplace_back(current_command.fd, -EBADF);
            if (waiters_[index].write_enabled)
              write_events_.emplace_back(current_command.fd, -EBADF);
          } break;
          case command_type::update_read: {
            assert(index < waiters_.size());                                         
            waiters_[index].read_enabled = true;
            waiters_[index].read_data = current_command.data;
          } break;
          case command_type::update_write: {
            assert(index < waiters_.size());                                         
            waiters_[index].write_enabled = true;
            waiters_[index].write_data = current_command.data;
          } break;
          case command_type::remove_read: {
            assert(index < waiters_.size());                                         
            waiters_[index].read_enabled = false;
          } break;
          case command_type::remove_write: {
            assert(index < waiters_.size());                                         
            waiters_[index].write_enabled = false;
          } break;
        }
      }

      // Dispatch the events
      for (auto const& read_event : read_events_) {
        if (waiters_[std::get<0>(read_event)].read_enabled)
          handler_.read(waiters_[std::get<0>(read_event)].read_data, std::get<1>(read_event));
      }
      read_events_.clear();

      for (auto const& write_event : write_events_) {
        if (waiters_[std::get<0>(write_event)].write_enabled)
          handler_.write(waiters_[std::get<0>(write_event)].write_data, std::get<1>(write_event));
      }
      write_events_.clear();

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

  template <class Duration>
  io_loop_end_reason loop(int nb_iter, Duration&& duration) {
    return this->loop(
        nb_iter, std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
  }
};

}
}

#endif  // BOSON_NETPOLLER_H_
