#include "client/UnixSocketClient.hpp"

#include <stdexcept>
#include <sys/socket.h>

UnixSocketClient::UnixSocketClient(const std::string &path_name)
    : UnixSocket(path_name) {}

void UnixSocketClient::connect() {
  if (::connect(_fd, reinterpret_cast<sockaddr *>(&_addr),
                sizeof(sockaddr_un)) == -1) {
    throw std::runtime_error(std::string("connect: ") + strerror(errno));
  }
}
