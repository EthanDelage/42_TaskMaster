#include "server/ClientSession.hpp"

#include <common/Logger.hpp>
#include <cstring>
#include <stdexcept>
#include <unistd.h>

char ClientSession::_buffer[SOCKET_BUFFER_SIZE];

ClientSession::ClientSession(const int client_fd)
    : Socket(client_fd), _reload_request(false) {}

std::string ClientSession::recv_command() const {
  std::string buffer_str;
  ssize_t ret;
  size_t endl_pos;

  ret = read(_buffer, sizeof(_buffer));
  if (ret == -1) {
    Logger::get_instance().error("Failed to read command from client fd=" +
                                 std::to_string(_fd) + ": " + strerror(errno));
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
  Logger::get_instance().info("Read " + std::to_string(ret) +
                              " bytes from fd=" + std::to_string(_fd) + ": `" +
                              buffer_str + '`');
  return buffer_str;
}

void ClientSession::send_response(const std::string &response) const {
  if (write(response) == -1) {
    throw std::runtime_error("send_response()");
  }
}

bool ClientSession::get_reload_request() const { return _reload_request; }

void ClientSession::set_reload_request(const bool reload) {
  _reload_request = reload;
}
