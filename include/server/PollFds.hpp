#ifndef POLLFDS_HPP
#define POLLFDS_HPP
#include <sys/poll.h>
#include <vector>

class PollFds {
public:
  enum class FdType {
    Server,
    Client,
    Process,
    WakeUp,
  };

  typedef struct metadata_s {
    FdType type;
  } metadata_t;

  typedef struct snapshot_s {
    std::vector<pollfd> poll_fds;
    std::vector<metadata_t> metadata;
  } snapshot_t;

  PollFds();

  void add_poll_fd(pollfd fd, metadata_t metadata);
  void remove_poll_fd(int fd);

  std::mutex &get_mutex();
  snapshot_t get_snapshot();

private:
  std::vector<pollfd> _poll_fds;
  std::vector<metadata_t> _metadata;
  std::mutex _mutex;
};

#endif // POLLFDS_HPP
