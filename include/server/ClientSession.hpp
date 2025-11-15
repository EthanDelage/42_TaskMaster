#ifndef CLIENTSESSION_HPP
#define CLIENTSESSION_HPP

#include "common/socket/Socket.hpp"

#include <string>

class ClientSession : public Socket {

public:
  explicit ClientSession(int client_fd);

  std::string recv_command() const;
  void send_response(const std::string &response) const;

  bool get_reload_request() const;
  void set_reload_request(bool reload);

private:
  static char _buffer[SOCKET_BUFFER_SIZE];
  bool _reload_request;
};

#endif // CLIENTSESSION_HPP
