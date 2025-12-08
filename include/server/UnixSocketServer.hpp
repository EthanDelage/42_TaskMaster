#ifndef UNIXSOCKETSERVER_HPP
#define UNIXSOCKETSERVER_HPP

#include "common/socket/UnixSocket.hpp"

#include <vector>

class UnixSocketServer : public UnixSocket {
public:
  explicit UnixSocketServer(const std::string &path_name);
  ~UnixSocketServer();

  int accept_client();
  int listen(int backlog) const;
};

#endif // UNIXSOCKETSERVER_HPP
