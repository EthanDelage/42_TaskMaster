#ifndef PROCESS_HPP
#define PROCESS_HPP

#include <string>
#include "server/config/ProgramConfig.hpp"

class Process {
public:
  explicit Process(ProgramConfig& config);

  int start();
  int stop(int sig);
  int restart(int sig);

  pid_t get_pid() const;
  ProgramConfig& get_program_config();

private:
  static std::string get_cmd_path(const std::string &cmd);

  ProgramConfig _program_config;
  std::string _cmd_path;
  pid_t _pid;
};

#endif // PROCESS_HPP
