#include "client/UnixSocketClient.hpp"

#include <common/Logger.hpp>
#include <stdexcept>
#include <sys/socket.h>

UnixSocketClient::UnixSocketClient(const std::string &path_name)
    : UnixSocket(path_name) {}

void UnixSocketClient::connect() {
  Logger::get_instance().info("Connecting to server...");
  if (::connect(_fd, reinterpret_cast<sockaddr *>(&_addr),
                sizeof(sockaddr_un)) == -1) {
    Logger::get_instance().error(std::string("Failed to connect to server: ") +
                                 strerror(errno));
    throw std::runtime_error(std::string("connect: ") + strerror(errno));
  }
  Logger::get_instance().info("Connected (fd=" + std::to_string(_fd) + ')');
}
