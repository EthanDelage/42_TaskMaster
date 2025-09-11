#ifndef UNIXSOCKET_HPP
#define UNIXSOCKET_HPP

#include <string>
#include <sys/un.h>

#define BACKLOG 5
#define SOCKET_PATH_NAME "/tmp/taskmasterd"
#define SOCKET_BUFFER_SIZE 1024

class UnixSocket {
public:
  explicit UnixSocket(const std::string &path_name);
  ~UnixSocket();

  int send(const std::string &message) const;
  std::string receive() const;

  int get_fd() const;
  sockaddr_un get_sockaddr() const;

protected:
  int _fd;
  sockaddr_un _addr{};
};

#endif // UNIXSOCKET_HPP
