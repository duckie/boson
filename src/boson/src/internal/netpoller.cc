#include "internal/netpoller.h"
#include "io_event_loop_impl.h"

namespace boson {
namespace internal {

netpoller_platform_impl::netpoller_platform_impl(io_event_handler& handler)
    : loop_{new io_event_loop(handler, 0)}
{
}

void netpoller_platform_impl::register_fd(fd_t fd, event_data data)
{
  loop_->register_fd(fd, data);
}

void netpoller_platform_impl::unregister(fd_t fd)
{
  loop_->unregister(fd);
}

}
}
