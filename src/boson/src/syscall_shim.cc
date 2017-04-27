#include <iostream>
#include <unistd.h>
#include <sys/syscall.h>

namespace boson {

namespace internal {
  class thread;
  thread*& current_thread();
}

int open(const char *, int);

}

extern "C" {
int open(const char *pathname, int flags) {
  if (boson::internal::current_thread()) {
    std::cout << "Yaye" << std::endl;
    return boson::open(pathname, flags);
  }
  return ::syscall(SYS_open, pathname, flags, 0755);
}
}

//fd_t open(const char *pathname, int flags, mode_t mode) {
  //return boson::open(pathname, flags, mode);
//}
//
//fd_t creat(const char *pathname, mode_t mode) {
  //return boson::creat(pathname, mode);
//}
//
//int pipe(fd_t (&fds)[2]) {
  //return boson::pipe(fds);
//}
//
//int pipe2(fd_t (&fds)[2], int flags) {
  //return boson::pipe2(fds);
//}
//
//socket_t socket(int domain, int type, int protocol) {
  //return boson::socket(domain, type, protocol);
//}
//
//ssize_t read(fd_t fd, void *buf, size_t count) {
  //return boson::read(fd,buf,count);
//}
//
//ssize_t write(fd_t fd, const void *buf, size_t count) {
  //return boson::write(fd,buf,count);
//}
//
//socket_t accept(socket_t socket, sockaddr *address, socklen_t *address_len) {
  //return boson::accept(socket, address, address_len);
//}
//
//int connect(socket_t sockfd, const sockaddr *addr, socklen_t addrlen) {
  //return boson::connect(sockfd, addr, addrlen);
//}
//
//ssize_t send(socket_t socket, const void *buffer, size_t length, int flags) {
  //return boson::send(socket, buffer, length, flags);
//}
//
//ssize_t recv(socket_t socket, void *buffer, size_t length, int flags) {
  //return boson::recv(socket, buffer, length, flags);
//}
//
//int close(int fd) {
  //return boson::close(fd);
//}
