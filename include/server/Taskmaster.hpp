#ifndef TASKMASTER_HPP
#define TASKMASTER_HPP

#include "server/Process.hpp"
#include "server/config/Config.hpp"

#include <unordered_map>

class Taskmaster {
public:
  explicit Taskmaster(Config config);
  void loop();

private:
  Config _config;
  std::unordered_map<std::string, Process> _processes;
  std::unordered_map<pid_t, Process &> _running_processes;

  void start_process(Process &process);
  void stop_process(Process &process);
  void autostart_process(Process &process);
  void autostart_processes();
  void reap_processes();
  void process_termination_handler(pid_t pid, int exitcode);
  void signal_interruption_handler(void);
  void update_config();
};

#endif // TASKMASTER_HPP
