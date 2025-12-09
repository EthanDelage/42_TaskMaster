#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <string>

#define SOCKET_BUFFER_SIZE 1024

class Socket {

public:
  explicit Socket(int fd);

  ssize_t read(char *buffer, size_t size) const;
  ssize_t write(const std::string &buffer) const;
  ssize_t write(const char *buffer, size_t size) const;
  static ssize_t read(int fd, char *buffer, size_t size);
  static ssize_t write(int fd, const std::string &buffer);
  static ssize_t write(int fd, const char *buffer, size_t size);

  int get_fd() const;

protected:
  int _fd;
};

#endif // SOCKET_HPP
