#include "internal/netpoller.h"
#include "io_event_loop_impl.h"

namespace boson {
namespace internal {

netpoller::netpoller(event_handler& handler)
    : loop_{new io_event_loop(*this, 0)},
      handler_{handler}

{
}

void netpoller::read(int fd, void* data, event_status status) {
}

void netpoller::write(int fd, void* data, event_status status) {
}

}
}
