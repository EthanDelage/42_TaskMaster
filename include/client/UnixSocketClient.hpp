#ifndef UNIXSOCKETCLIENT_HPP
#define UNIXSOCKETCLIENT_HPP
#include "common/UnixSocket.hpp"

class UnixSocketClient : public UnixSocket {
public:
  UnixSocketClient(const std::string &path_name);

  void connect();
};

#endif // UNIXSOCKETCLIENT_HPP
