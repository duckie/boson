#ifndef BOSON_NETPOLLER_H_
#define BOSON_NETPOLLER_H_

#include "io_event_loop.h"
#include "event_loop.h"
#include "queues/mpsc.h"
#include "thread.h"

namespace boson {
namespace internal {

struct netpoller_platform_impl {
  std::unique_ptr<io_event_loop> loop_;
  
  netpoller_platform_impl(io_event_handler& handler);
  void register_fd(fd_t fd, event_data data);
  void unregister(fd_t fd);
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
    Data read_data;
    Data write_data;
  };

  enum class command_type { new_fd, close_fd, update_read, update_write };

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
  queues::mpsc<command> pending_updates_;

 public:
  netpoller(net_event_handler<Data>& handler) : netpoller_platform_impl{*this} {}

  void read(event_data data, event_status status) override {

  }
  void write(event_data data, event_status status) override {}

  void signal_new_fd(fd_t fd) {
    pending_updates_.write(command { command_type::new_fd, fd, Data{} });
    register_fd(fd, {.fd = fd});
  }

  void signal_fd_closed(fd_t fd) {
    pending_updates_.write(command { command_type::close_fd, fd, Data{} });
    unregister(fd);
  }

  void register_read(fd_t fd, Data value) {
    pending_updates_.write(command { command_type::update_read, fd, value });
  }

  void register_write(fd_t fd, Data value) {
    pending_updates_.write(command { command_type::update_write, fd, value });
  }

  void loop() {
    // Unqueue the commands
    command current_command;
    while (pending_updates_.read(current_command)) {
      size_t index = static_cast<size_t>(current_command.fd);
      switch (current_command.type) {
        case command_type::new_fd: {
          if (waiters_.size() <= index) {
            waiters_.resize(index + 1);
          }
        } break;
        case command_type::close_fd: {
          waiters_[index] = fd_data{ Data{}, Data{} };
        } break;
        case command_type::update_read: {
          waiters_[index].read_data = current_command.data; 
        } break;
        case command_type::update_write: {
          waiters_[index].write_data = current_command.data;
        } break;
      }
    }
  }
};

}
}

#endif  // BOSON_NETPOLLER_H_
