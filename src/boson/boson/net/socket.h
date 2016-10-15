#ifndef BOSON_NET_SOCKET_H_
#define BOSON_NET_SOCKET_H_

#include "boson/system.h"

namespace boson {
namespace net {

socket_t create_listening_socket(
    int port,
    int max_connections = 1e5,
    int domain = AF_INET,
    int type = SOCK_STREAM,
    int protocol = 0,
    int non_block = true,
    in_addr_t receive_from=INADDR_ANY);

}  // namespace net
}  // namespace boson

#endif
