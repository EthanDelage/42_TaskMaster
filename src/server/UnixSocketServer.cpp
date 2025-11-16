#include "server/UnixSocketServer.hpp"

#include <common/Logger.hpp>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

UnixSocketServer::UnixSocketServer(const std::string &path_name)
    : UnixSocket(path_name) {
  if (remove(path_name.c_str()) == -1 && errno != ENOENT) {
    Logger::get_instance().error("Failed to remove `" + path_name +
                                 "`: " + strerror(errno));
    throw std::runtime_error(std::string("unix socket: ") + strerror(errno));
  }
  if (bind(_fd, reinterpret_cast<sockaddr *>(&_addr), sizeof(_addr)) == -1) {
    Logger::get_instance().error(
        std::string("Failed to bind the server socket: ") + strerror(errno));
    throw std::runtime_error(std::string("bind: ") + strerror(errno));
  }
  Logger::get_instance().info(
      "Server socket successfully created (fd=" + std::to_string(_fd) + ")");
}

UnixSocketServer::~UnixSocketServer() {
  for (auto &fd : _listen_fds) {
    if (close(fd) == -1) {
      perror("close");
    }
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
  _listen_fds.push_back(client_fd);
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
