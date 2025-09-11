#ifndef PROCESS_HPP
#define PROCESS_HPP

#include "server/config/ProgramConfig.hpp"
#include <string>

enum class State {
  IDLE,
  RUNNING,
  STOPPING,
  FINISHED,
  CRASHED,
};

class Process {
public:
  explicit Process(ProgramConfig &program_config);

  int start();
  int stop(int sig);
  int restart(int sig);

  pid_t get_pid() const;
  ProgramConfig &get_program_config();
  State get_state() const;

private:
  static std::string get_cmd_path(const std::string &cmd);

  ProgramConfig _program_config;
  std::string _cmd_path;
  pid_t _pid;
  State _state;
};

#endif // PROCESS_HPP
