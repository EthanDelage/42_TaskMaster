#include "server/ClientSession.hpp"

#include <stdexcept>
#include <unistd.h>

char ClientSession::_buffer[BUFFER_SIZE];

ClientSession::ClientSession(const int client_fd) : _sock_fd(client_fd) {}

ClientSession::~ClientSession() { close(_sock_fd); }

std::string ClientSession::recv_command() const {
  std::string buffer_str;
  ssize_t ret;
  size_t endl_pos;

  ret = read(_sock_fd, _buffer, sizeof(_buffer));
  if (ret == -1) {
    throw std::runtime_error("read_command()");
  }
  if (ret == 0) {
    throw std::runtime_error("client disconnected");
  }
  buffer_str = std::string(_buffer, ret);
  endl_pos = buffer_str.find('\n');
  if (endl_pos != std::string::npos) {
    buffer_str.erase(endl_pos);
  }
  return buffer_str;
}

void ClientSession::send_response(const std::string &response) const {
  if (write(_sock_fd, response.c_str(), response.length()) == -1) {
    throw std::runtime_error("send_response()");
  }
}

int ClientSession::get_fd() const { return _sock_fd; }
