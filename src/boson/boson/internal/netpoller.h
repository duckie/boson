#ifndef BOSON_NETPOLLER_H_
#define BOSON_NETPOLLER_H_

#include "io_event_loop.h"
#include "event_loop.h"
#include "queues/mpsc.h"
#include "thread.h"

namespace boson {
namespace internal {

/** 
 * Implements the event_loop from boson perspective
 *
 * The io_event_loop is a simple loop made to contain platform specific
 * asyncrhonous event code.
 *
 * The netpoller extends said loop to implements boson's behavior and policy
 * regarding fd management.
 */
class netpoller : public io_event_handler {
  std::unique_ptr<io_event_loop> loop_;
  event_handler& handler_;

  struct fd_data {
    thread_id thread_read;
    size_t idx_read;
    thread_id thread_write;
    size_t idx_write;
  };

  enum class command_type {
    update_read, update_write
  };

  struct command {
    command_type type;
    thread_id thread;
    size_t idx;
  };

  std::vector<fd_data> waiters_;
  queues::mpsc<command> pending_updates_;

 public:
  netpoller(event_handler& handler);
  void read(int fd, void* data, event_status status) override;
  void write(int fd, void* data, event_status status) override;
};

}
}

#endif  // BOSON_NETPOLLER_H_
