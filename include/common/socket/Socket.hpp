#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <string>

#define SOCKET_BUFFER_SIZE 1024

class Socket {

public:
  explicit Socket(int fd);
  ~Socket();

  ssize_t read(char *buffer, size_t size) const;
  ssize_t write(const std::string &buffer) const;

  int get_fd() const;

protected:
  int _fd;
};

#endif // SOCKET_HPP
