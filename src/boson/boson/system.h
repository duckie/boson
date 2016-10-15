#ifndef BOSON_SYSTEM_H_
#define BOSON_SYSTEM_H_

#ifdef WINDOWS
#include <winsock2.h> 
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <netdb.h>
//#define closesocket(s) close(s)
typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;
typedef struct in_addr IN_ADDR;
#endif

#include <chrono>
#include <cstdint>

namespace boson {
#ifdef WINDOWS
using fd_t = HANDLE;
using sockaddr_in = SOCKADDR_IN;
using sockaddr = SOCKADDR;
using in_addr = IN_ADDR;
#else

static constexpr int const INVALID_SOCKET = -1;
static constexpr int const SOCKET_ERROR = -1;
using fd_t = int;
using socket_t = int;
using sockaddr_in = ::sockaddr_in;
using sockaddr = ::sockaddr;
using in_addr = ::in_addr;

#endif
}  // namespace boson

#endif  // BOSON_SYSCALLS_H_
