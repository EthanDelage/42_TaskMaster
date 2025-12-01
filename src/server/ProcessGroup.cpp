#include "server/ProcessGroup.hpp"
#include <iostream>

ProcessGroup::ProcessGroup(process_config_t&& config) {
  _config = std::make_shared<process_config_t>(std::move(config));
  for (size_t i = 0; i < _config->numprocs; ++i) {
    _process_vector.emplace_back(_config);
  }
}

process_config_t const &ProcessGroup::get_process_config() const { return *_config; }

void ProcessGroup::stop(const int sig) {
  for (Process &process : _process_vector) {
    if (process.get_status().running) {
      process.stop(sig);
    }
  }
}

void ProcessGroup::start() {
  for (Process &process : _process_vector) {
    process.start();
  }
}

std::vector<Process>::iterator ProcessGroup::begin() { return _process_vector.begin(); }

std::vector<Process>::const_iterator ProcessGroup::begin() const { return _process_vector.begin(); }

std::vector<Process>::const_iterator ProcessGroup::cbegin() const { return _process_vector.cbegin(); }

std::vector<Process>::iterator ProcessGroup::end() { return _process_vector.end(); }

std::vector<Process>::const_iterator ProcessGroup::end() const { return _process_vector.end(); }

std::vector<Process>::const_iterator ProcessGroup::cend() const { return _process_vector.cend(); }

std::ostream &operator<<(std::ostream &os, const ProcessGroup &process_group) {
  os << "[" << process_group.get_process_config().name << "] ";
  os << "#" << process_group.get_process_config().numprocs << std::endl;
  for (const Process &process : process_group) {
    os << '\t' << process << std::endl;
  }
  return os;
}
