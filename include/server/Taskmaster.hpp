#ifndef TASKMASTER_HPP
#define TASKMASTER_HPP

#include "server/Process.hpp"
#include "server/config/Config.hpp"

#include <unordered_map>

class Taskmaster {
public:
  explicit Taskmaster(Config config);
  int autostart_processes();
  void loop();

private:
  std::vector<Process> _processes;
  std::unordered_map<pid_t, Process &> _running_processes;

  void reap_processes();
};

#endif // TASKMASTER_HPP
