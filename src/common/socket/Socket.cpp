#include "common/socket/Socket.hpp"

#include <iostream>
#include <unistd.h>

Socket::Socket(const int fd) : _fd(fd) {}

Socket::~Socket() { close(_fd); }

ssize_t Socket::read(char *buffer, size_t size) const {
  return ::read(_fd, buffer, size);
}

ssize_t Socket::write(const std::string &buffer) const {
  return ::write(_fd, buffer.c_str(), buffer.size());
}

int Socket::get_fd() const { return _fd; }
