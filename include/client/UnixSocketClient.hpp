#ifndef UNIXSOCKETCLIENT_HPP
#define UNIXSOCKETCLIENT_HPP
#include "common/socket/UnixSocket.hpp"

class UnixSocketClient : public UnixSocket {
public:
  explicit UnixSocketClient(const std::string &path_name);

  void connect();
};

#endif // UNIXSOCKETCLIENT_HPP
