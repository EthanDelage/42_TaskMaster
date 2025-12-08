#include "common/socket/UnixSocket.hpp"

#include "common/Logger.hpp"

#include <iostream>
#include <ostream>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

UnixSocket::UnixSocket(const std::string &path_name)
    : Socket(socket(AF_UNIX, SOCK_STREAM, 0)) {
  if (_fd == -1) {
    Logger::get_instance().error(
        std::string("UnixSocket::UnixSocket(): socket creation failed: ") +
        strerror(errno));
    throw std::runtime_error(std::string("socket: ") + strerror(errno));
  }
  Logger::get_instance().info(
      "UnixSocket::UnixSocket(): Socket successfully created fd=" +
      std::to_string(_fd));
  memset(&_addr, 0, sizeof(sockaddr_un));
  _addr.sun_family = AF_UNIX;
  strncpy(_addr.sun_path, path_name.c_str(), sizeof(_addr.sun_path) - 1);
}

sockaddr_un UnixSocket::get_sockaddr() const { return _addr; }
