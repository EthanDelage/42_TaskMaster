#ifndef CLIENTSESSION_HPP
#define CLIENTSESSION_HPP
#include <string>

#define BUFFER_SIZE 1024

class ClientSession {

public:
  explicit ClientSession(int client_fd);
  ~ClientSession();
  std::string recv_command() const;
  void send_response(const std::string &response) const;

  int get_fd() const;

private:
  int _sock_fd;
  static char _buffer[BUFFER_SIZE];
};

#endif // CLIENTSESSION_HPP
