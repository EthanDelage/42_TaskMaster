#ifndef PROCESSGROUP_HPP
#define PROCESSGROUP_HPP

#include "server/Process.hpp"
#include <vector>

class ProcessGroup {
public:
  explicit ProcessGroup(process_config_t &&config);
  process_config_t const &get_process_config() const;

  void stop(int sig);
  void start();

  std::vector<Process>::iterator begin();
  std::vector<Process>::const_iterator begin() const;
  std::vector<Process>::const_iterator cbegin() const;
  std::vector<Process>::iterator end();
  std::vector<Process>::const_iterator end() const;
  std::vector<Process>::const_iterator cend() const;

private:
  std::vector<Process> _process_vector;
  std::shared_ptr<const process_config_t> _config;
};

std::ostream &operator<<(std::ostream &os, const Process &process);
std::ostream &operator<<(std::ostream &os, const ProcessGroup &process_group);

#endif // PROCESSGROUP_HPP
