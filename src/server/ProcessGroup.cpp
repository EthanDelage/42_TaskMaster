#include "server/ProcessGroup.hpp"
#include "common/Logger.hpp"
#include <iostream>

ProcessGroup::ProcessGroup(process_config_t &&config) {
  _config = std::make_shared<process_config_t>(std::move(config));
  for (size_t i = 0; i < _config->numprocs; ++i) {
    _process_vector.emplace_back(_config);
  }
}

process_config_t const &ProcessGroup::get_process_config() const {
  return *_config;
}

void ProcessGroup::stop(const int sig) {
  Logger::get_instance().info("Stopping " + str());
  for (Process &process : _process_vector) {
    if (process.get_status().running) {
      process.stop(sig);
      process.send_message_to_client(process.str() + " has stopped\n");
    }
  }
}

void ProcessGroup::start() {
  Logger::get_instance().info("Starting" + str());
  for (Process &process : _process_vector) {
    process.start();
  }
}

std::string ProcessGroup::str() const {
  return "pgroup [" + _config->name + "]#" + std::to_string(_config->numprocs);
}

ProcessGroup::GroupIterator ProcessGroup::begin() {
  return _process_vector.begin();
}

ProcessGroup::GroupConstIterator ProcessGroup::begin() const {
  return _process_vector.begin();
}

ProcessGroup::GroupConstIterator ProcessGroup::cbegin() const {
  return _process_vector.cbegin();
}

ProcessGroup::GroupIterator ProcessGroup::end() {
  return _process_vector.end();
}

ProcessGroup::GroupConstIterator ProcessGroup::end() const {
  return _process_vector.end();
}

ProcessGroup::GroupConstIterator ProcessGroup::cend() const {
  return _process_vector.cend();
}

std::ostream &operator<<(std::ostream &os, const ProcessGroup &process_group) {
  os << process_group.str() << std::endl;
  for (const Process &process : process_group) {
    os << '\t' << process << std::endl;
  }
  return os;
}
