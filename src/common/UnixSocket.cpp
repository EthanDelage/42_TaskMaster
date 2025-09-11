#include "common/UnixSocket.hpp"

#include <iostream>
#include <ostream>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

UnixSocket::UnixSocket(const std::string &path_name) {
  _fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (_fd == -1) {
    throw std::runtime_error(std::string("socket: ") + strerror(errno));
  }
  memset(&_addr, 0, sizeof(sockaddr_un));
  _addr.sun_family = AF_UNIX;
  strncpy(_addr.sun_path, path_name.c_str(), sizeof(_addr.sun_path) - 1);
}

UnixSocket::~UnixSocket() { close(_fd); }

int UnixSocket::send(const std::string &message) const {
  if (write(_fd, message.c_str(), message.size()) == -1) {
    return -1;
  }
  return 0;
}

std::string UnixSocket::receive() const {
  ssize_t ret_val = SOCKET_BUFFER_SIZE;
  char buffer[SOCKET_BUFFER_SIZE];
  std::string response;

  while (ret_val == SOCKET_BUFFER_SIZE) {
    ret_val = read(_fd, &buffer, SOCKET_BUFFER_SIZE);
    std::cout << "read successful " << ret_val << std::endl;
    if (ret_val == -1) {
      throw std::runtime_error(std::string("read: ") + strerror(errno));
    }
    response.append(buffer, ret_val);
  }
  return response;
}

int UnixSocket::get_fd() const { return _fd; }

sockaddr_un UnixSocket::get_sockaddr() const { return _addr; }
