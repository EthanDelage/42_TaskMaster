#ifndef PROCESS_HPP
#define PROCESS_HPP

#include "server/ConfigParser.hpp"
#include <chrono>
#include <memory>

#define PIPE_READ 0
#define PIPE_WRITE 1

class Process {
public:
  typedef struct {
    bool running;
    bool killed;
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

  Process(std::shared_ptr<const process_config_t> process_config);
  ~Process();

  int start();
  int stop(int sig);
  int kill();
  int update_status(void);
  bool check_autorestart(void);

  const process_config_t &get_process_config() const;
  pid_t get_pid() const;
  std::chrono::steady_clock::time_point get_start_timestamp() const;
  size_t get_num_retries() const;
  State get_state() const;
  State get_previous_state() const;
  status_t get_status() const;
  Command get_pending_command() const;
  const int *get_stdout_pipe() const;
  const int *get_stderr_pipe() const;
  unsigned long get_runtime(void);
  unsigned long get_stoptime(void);

  void set_num_retries(size_t startretries);
  void set_state(State state);
  void set_previous_state(State state);
  void set_pending_command(Command command);

private:
  void setup();
  void setup_env() const;
  void setup_workingdir() const;
  void setup_umask() const;
  void setup_outputs();

  std::shared_ptr<const process_config_s> _process_config;
  pid_t _pid;
  std::chrono::steady_clock::time_point _start_timestamp;
  std::chrono::steady_clock::time_point _stop_timestamp;
  size_t _num_retries;
  State _state;
  State _previous_state;
  status_t _status;
  Command _pending_command;
  int _stdout_pipe[2];
  int _stderr_pipe[2];
  int _stdout_fd;
  int _stderr_fd;
  std::vector<int> _attached_client;
};

std::ostream &operator<<(std::ostream &os, const Process &process);

std::ostream &operator<<(std::ostream &os, const Process::State &state);

#endif // PROCESS_HPP
