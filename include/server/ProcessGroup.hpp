#ifndef PROCESSGROUP_HPP
#define PROCESSGROUP_HPP

#include "server/Process.hpp"
#include <vector>

class ProcessGroup {
  using GroupType = std::vector<Process>;
  using GroupIterator = GroupType::iterator;
  using GroupConstIterator = GroupType::const_iterator;

public:
  explicit ProcessGroup(process_config_t &&config);
  process_config_t const &get_process_config() const;

  void stop(int sig);
  void start();

  GroupIterator begin();
  GroupConstIterator begin() const;
  GroupConstIterator cbegin() const;
  GroupIterator end();
  GroupConstIterator end() const;
  GroupConstIterator cend() const;

private:
  std::vector<Process> _process_vector;
  std::shared_ptr<const process_config_t> _config;
};

std::ostream &operator<<(std::ostream &os, const ProcessGroup &process_group);

#endif // PROCESSGROUP_HPP
