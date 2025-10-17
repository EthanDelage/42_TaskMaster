#ifndef PROCESS_HPP
#define PROCESS_HPP

#include "server/config/ProgramConfig.hpp"
#include <chrono>
#include <memory>
#include <string>

#define PIPE_READ 0
#define PIPE_WRITE 1


class Process {
public:
  typedef struct {
    bool running;
    bool exited;
    bool signaled;
    int exitstatus;
  } status_t;

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

  Process(std::shared_ptr<const ProgramConfig> program_config);
  ~Process();

  int start();
  int stop(int sig);
  int kill();
  int update_status(void);
  bool check_autorestart(void);
  unsigned long get_runtime(void);
  unsigned long get_stoptime(void);

  const ProgramConfig &get_program_config();
  pid_t get_pid() const;
  std::chrono::steady_clock::time_point get_start_timestamp() const;
  size_t get_num_retries() const;
  State get_state() const;
  status_t get_status() const;
  Command get_pending_command() const;
  const int *get_stdout_pipe() const;
  const int *get_stderr_pipe() const;

  void set_num_retries(size_t startretries);
  void set_state(State state);
  void set_pending_command(Command);

private:
  static std::string get_cmd_path(const std::string &cmd);
  void setup_env() const;
  void setup_workingdir() const;
  void setup_outputs();

  std::shared_ptr<const ProgramConfig> _program_config;
  pid_t _pid;
  std::chrono::steady_clock::time_point _start_timestamp;
  std::chrono::steady_clock::time_point _stop_timestamp;
  size_t _num_retries;
  State _state;
  status_t _status;
  Command _pending_command;
  int _stdout_pipe[2];
  int _stderr_pipe[2];
  int _stdout_fd;
  int _stderr_fd;
  std::vector<int> _attached_client;
  std::string _cmd_path;
};

#endif // PROCESS_HPP
