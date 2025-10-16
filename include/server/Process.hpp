#ifndef PROCESS_HPP
#define PROCESS_HPP

#include "server/config/ProgramConfig.hpp"
#include <chrono>
#include <memory>
#include <string>

#define PIPE_READ   0
#define PIPE_WRITE  1

class Process {
public:
  enum class State {
    Waiting,
    Starting,
    Running,
    Exiting,
    Stopped,
  };

  enum class Command {
    Stop,
    Start,
    Restart,
    None,
  };

  Process(std::shared_ptr<ProgramConfig> program_config);
  ~Process();

  int start();
  int stop(int sig);
  int restart(int sig);

  pid_t get_pid() const;
  ProgramConfig &get_program_config();
  std::chrono::steady_clock::time_point get_start_time() const;
  size_t get_num_retries() const;
  State get_state() const;
  const int *get_stdout_pipe() const;
  const int *get_stderr_pipe() const;

  void set_num_retries(size_t startretries);
  void set_state(State state);
  void set_requested_command(Command);

private:
  static std::string get_cmd_path(const std::string &cmd);
  void setup_env() const;
  void setup_workingdir() const;
  void setup_outputs();

  std::shared_ptr<ProgramConfig> _program_config;
  pid_t _pid;
  std::chrono::steady_clock::time_point _start_time;
  size_t _num_retries;
  State _state;
  Command _requested_command;
  int _stdout_pipe[2];
  int _stderr_pipe[2];
  int _stdout_fd;
  int _stderr_fd;
  std::vector<int> _attached_client;
  std::string _cmd_path;
};

#endif // PROCESS_HPP
