#ifndef PROCESS_HPP
#define PROCESS_HPP

#include "server/config/ProgramConfig.hpp"
#include <chrono>
#include <memory>
#include <string>

class Process {
public:
  Process(std::shared_ptr<ProgramConfig> program_config);

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

  std::shared_ptr<ProgramConfig> _program_config;
  pid_t _pid;
  std::chrono::steady_clock::time_point _start_time;
  unsigned long _startretries;
  int _stdout_pipe[2];
  int _stderr_pipe[2];
  std::string _cmd_path;
};

#endif // PROCESS_HPP
