#include "server/UnixSocketServer.hpp"

#include <common/Logger.hpp>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

UnixSocketServer::UnixSocketServer(const std::string &path_name)
    : UnixSocket(path_name) {
  if (unlink(path_name.c_str()) == -1 && errno != ENOENT) {
    Logger::get_instance().error("Failed to unlink `" + path_name +
                                 "`: " + strerror(errno));
    throw std::runtime_error(std::string("unix socket: ") + strerror(errno));
  }
  if (bind(_fd, reinterpret_cast<sockaddr *>(&_addr), sizeof(_addr)) == -1) {
    Logger::get_instance().error(
        std::string("Failed to bind the server socket: ") + strerror(errno));
    throw std::runtime_error(std::string("bind: ") + strerror(errno));
  }
  if (chmod(path_name.c_str(), 0666) == -1) {
    Logger::get_instance().error(
        std::string("UnixSocketServer: failed to chmod: ") + strerror(errno));
    throw std::runtime_error(std::string("chmod: ") + strerror(errno));
  }
  Logger::get_instance().info(
      "Server socket successfully created (fd=" + std::to_string(_fd) + ")");
}

UnixSocketServer::~UnixSocketServer() {
  if (close(_fd) == -1) {
    Logger::get_instance().error(
        std::string(
            "UnixSocketServer destructor: Failed to close server socket: ") +
        strerror(errno));
  }
  if (unlink(_addr.sun_path) == -1) {
    Logger::get_instance().error(std::string("Failed to unlink `") +
                                 _addr.sun_path + "`: " + strerror(errno));
  }
}

int UnixSocketServer::accept_client() {
  int client_fd = accept(_fd, nullptr, nullptr);
  if (client_fd == -1) {
    Logger::get_instance().error(
        std::string("Failed to handle client connection: ") + strerror(errno));
    return -1;
  }
  Logger::get_instance().info("Client fd=" + std::to_string(client_fd) +
                              " connected");
  return client_fd;
}

int UnixSocketServer::listen(int backlog) const {
  if (::listen(_fd, backlog) == -1) {
    perror("listen");
    return -1;
  }
  Logger::get_instance().info("Start listening from incoming connection...");
  return 0;
}
