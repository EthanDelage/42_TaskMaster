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

  void start_process(Process &process);
  void reap_processes();
  void process_termination_handler(pid_t pid, int exitcode);
};

#endif // TASKMASTER_HPP
