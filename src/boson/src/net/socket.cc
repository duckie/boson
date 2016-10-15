#include "boson/net/socket.h"
#include "boson/exception.h"
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <cstring>

namespace boson {
namespace net {

socket_t create_listening_socket(
    int port,
    int max_connections,
    int domain,
    int type,
    int protocol,
    int non_block,
    in_addr_t receive_from) {

  sockaddr_in serv_addr;
  int sockfd = ::socket(domain, type, protocol);
  if (non_block) ::fcntl(sockfd, F_SETFL, O_NONBLOCK);
  if (sockfd < 0) throw boson::exception("ERROR opening socket");

  ::memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = domain;
  serv_addr.sin_addr.s_addr = receive_from;
  serv_addr.sin_port = htons(port);

  // re use socket
  int yes = 1;
  if (::setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
    throw boson::exception("setsockopt");

  // bind it
  if (::bind(sockfd, reinterpret_cast<sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0)
    throw boson::exception("ERROR on binding");

  listen(sockfd, max_connections);

  return sockfd;
}

    

}  // namespace net
}  // namespace boson
