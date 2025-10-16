#include "server/UnixSocketServer.hpp"

#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

UnixSocketServer::UnixSocketServer(const std::string &path_name)
    : UnixSocket(path_name) {
  if (remove(path_name.c_str()) == -1 && errno != ENOENT) {
    throw std::runtime_error(std::string("unix socket: ") + strerror(errno));
  }
  if (bind(_fd, reinterpret_cast<sockaddr *>(&_addr), sizeof(_addr)) == -1) {
    throw std::runtime_error(std::string("bind: ") + strerror(errno));
  }
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
    return -1;
  }
  _listen_fds.push_back(client_fd);
  return client_fd;
}

int UnixSocketServer::listen(int backlog) const {
  if (::listen(_fd, backlog) == -1) {
    perror("listen");
    return -1;
  }
  return 0;
}
