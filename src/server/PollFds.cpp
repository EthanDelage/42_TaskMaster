#include "server/PollFds.hpp"

PollFds::PollFds() {}

void PollFds::add_poll_fd(pollfd fd, metadata_t metadata) {
  _poll_fds.emplace_back(fd);
  _metadata.emplace_back(metadata);
}

void PollFds::remove_poll_fd(const int fd) {
  const auto it =
      std::find_if(_poll_fds.begin(), _poll_fds.end(),
                   [fd](pollfd poll_fd) { return fd == poll_fd.fd; });
  if (it == _poll_fds.end()) {
    throw std::invalid_argument("remove_poll_fd(): invalid fd");
  }
  const long index = std::distance(_poll_fds.begin(), it);
  _poll_fds.erase(it);
  _metadata.erase(_metadata.begin() + index);
}

std::mutex &PollFds::get_mutex() { return _mutex; }

PollFds::snapshot_t PollFds::get_snapshot() {
  std::lock_guard lock(_mutex);
  return {_poll_fds, _metadata};
}
