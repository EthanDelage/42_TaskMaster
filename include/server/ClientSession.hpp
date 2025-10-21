#ifndef CLIENTSESSION_HPP
#define CLIENTSESSION_HPP

#include "common/socket/Socket.hpp"

#include <string>

class ClientSession : public Socket {

public:
  explicit ClientSession(int client_fd);

  std::string recv_command() const;
  void send_response(const std::string &response) const;

private:
  static char _buffer[SOCKET_BUFFER_SIZE];
};

#endif // CLIENTSESSION_HPP
