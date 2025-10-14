#ifndef PROCESS_HPP
#define PROCESS_HPP

#include "server/config/ProgramConfig.hpp"
#include <chrono>
#include <string>

class Process {
public:
  explicit Process(ProgramConfig &program_config);

  int start();
  int stop(int sig);
  int restart(int sig);
  void setup_env() const;
  void setup_outputs() const;
  void setup_workingdir() const;

  pid_t get_pid() const;
  ProgramConfig &get_program_config();
  std::chrono::steady_clock::time_point get_start_time() const;
  unsigned long get_startretries() const;

  void set_startretries(unsigned long startretries);

private:
  static std::string get_cmd_path(const std::string &cmd);

  ProgramConfig _program_config;
  std::string _cmd_path;
  pid_t _pid;
  std::chrono::steady_clock::time_point _start_time;
  unsigned long _startretries;
};

#endif // PROCESS_HPP
