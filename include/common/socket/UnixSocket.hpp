#ifndef UNIXSOCKET_HPP
#define UNIXSOCKET_HPP

#include "Socket.hpp"

#include <string>
#include <sys/un.h>

#define BACKLOG 5
#define SOCKET_PATH_NAME "/tmp/taskmasterd.sock"

class UnixSocket : public Socket {
public:
  explicit UnixSocket(const std::string &path_name);

  sockaddr_un get_sockaddr() const;

protected:
  sockaddr_un _addr{};
};

#endif // UNIXSOCKET_HPP
