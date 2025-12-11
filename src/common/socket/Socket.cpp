#include "common/socket/Socket.hpp"

#include <iostream>
#include <unistd.h>

Socket::Socket(const int fd) : _fd(fd) {}

ssize_t Socket::read(char *buffer, size_t size) const {
  return ::read(_fd, buffer, size);
}

ssize_t Socket::write(const std::string &buffer) const {
  return write(_fd, buffer);
}

ssize_t Socket::write(const char *buffer, size_t size) const {
  return write(_fd, buffer, size);
}

ssize_t Socket::read(int fd, char *buffer, size_t size) {
  return ::read(fd, buffer, size);
}

ssize_t Socket::write(int fd, const std::string &buffer) {
  return write(fd, buffer.c_str(), buffer.size());
}

ssize_t Socket::write(int fd, const char *buffer, size_t size) {
  return ::write(fd, buffer, size);
}

int Socket::get_fd() const { return _fd; }
