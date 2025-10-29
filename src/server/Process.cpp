#include "server/Process.hpp"

#include "server/ConfigParser.hpp"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <signal.h>
extern "C" {
#include <fcntl.h>
#include <stdlib.h>
#include <sys/wait.h>
}

static void redirect_output(int new_fd, int current_fd);

extern char **environ; // envp

Process::Process(std::shared_ptr<const process_config_t> process_config)
    : _process_config(process_config),
      _pid(-1),
      _num_retries(0),
      _state(State::Waiting),
      _previous_state(State::Waiting),
      _pending_command(Command::None) {
  if (pipe(_stdout_pipe) == -1) {
    throw std::runtime_error("Error: Process() failed to create stdout pipe");
  }
  if (pipe(_stderr_pipe) == -1) {
    throw std::runtime_error("Error: Process() failed to create stderr pipe");
  }
  std::string stdout_path = _process_config->stdout;
  _stdout_fd = stdout_path.empty() ? open("/dev/null", O_WRONLY)
                                   : open(stdout_path.c_str(),
                                          O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (_stdout_fd == -1) {
    throw std::runtime_error(std::string("open") + strerror(errno));
  }
  std::string stderr_path = _process_config->stderr;
  _stderr_fd = stderr_path.empty() ? open("/dev/null", O_WRONLY)
                                   : open(stderr_path.c_str(),
                                          O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (_stderr_fd == -1) {
    throw std::runtime_error(std::string("open") + strerror(errno));
  }
  std::cout << "Process " << _process_config->name << " created"
            << std::endl;
}

Process::~Process() {
  close(_stdout_pipe[PIPE_READ]);
  close(_stdout_pipe[PIPE_WRITE]);
  close(_stderr_pipe[PIPE_READ]);
  close(_stderr_pipe[PIPE_WRITE]);
  close(_stdout_fd);
  close(_stderr_fd);
}

int Process::start() {
  std::cout << "[Taskmaster] Starting " << _process_config->name << " ..."
            << std::endl;
  _pid = fork();
  if (_pid == -1) {
    perror("fork");
    return -1;
  }
  if (_pid > 0) {
    // parent process
    _start_timestamp = std::chrono::steady_clock::now();
    return 0;
  }
  // child process
  setup_env();
  setup_outputs();
  setup_workingdir();
  if (execve(_process_config->cmd_path.c_str(), _process_config->cmd->we_wordv, environ) == -1) {
    perror("execve");
    return -1;
  }
  return 0;
}

int Process::stop(const int sig) {
  if (::kill(_pid, sig) == -1) {
    perror("kill");
    return -1;
  }
  _pid = -1;
  return 0;
}

int Process::kill() {
  if (::kill(_pid, SIGKILL) == -1) {
    perror("kill");
    return -1;
  }
  _pid = -1;
  return 0;
}

int Process::update_status(void) {
  int status;

  pid_t result = waitpid(_pid, &status, WNOHANG);
  if (result == -1) {
    // Here waitpid returned an error, it may be due to
    // this function being called without the process being started.
    perror("waitpid");
    return -1;
  }
  if (result == 0) {
    _status.running = true;
    return 0;
  }
  _status.running = false;
  _status.exited = WIFEXITED(status);
  _status.signaled = WIFSIGNALED(status);
  _status.exitstatus = WEXITSTATUS(status);
  return 0;
}

/**
 * @brief Return true if the process needs to be autorestarted.
 **/
bool Process::check_autorestart(void) {
  AutoRestart autorestart = _process_config->autorestart;
  if (autorestart == AutoRestart::True) {
    return true;
  } else if (autorestart == AutoRestart::Unexpected) {
    if (_status.signaled) {
      return false;
    }
    for (const auto it : _process_config->exitcodes) {
      if (it == static_cast<int>(_status.exitstatus)) {
        // If the status is found in the list of expected status
        return false;
      }
    }
    // If the status is unexpected
    return true;
  }
  return false;
}

unsigned long Process::get_runtime(void) {
  long runtime = std::chrono::duration_cast<std::chrono::seconds>(
                     std::chrono::steady_clock::now() - _start_timestamp)
                     .count();
  if (runtime < 0) {
    return 0;
  }
  return static_cast<unsigned long>(runtime);
}

unsigned long Process::get_stoptime(void) {
  long stoptime = std::chrono::duration_cast<std::chrono::seconds>(
                      std::chrono::steady_clock::now() - _stop_timestamp)
                      .count();
  if (stoptime < 0) {
    return 0;
  }
  return static_cast<unsigned long>(stoptime);
}

pid_t Process::get_pid() const { return _pid; }

const process_config_t &Process::get_process_config() { return *_process_config; }

std::chrono::steady_clock::time_point Process::get_start_timestamp() const {
  return _start_timestamp;
}

size_t Process::get_num_retries() const { return _num_retries; }

Process::State Process::get_state() const { return _state; }

Process::State Process::get_previous_state() const { return _previous_state; }

Process::status_t Process::get_status() const { return _status; }

Process::Command Process::get_pending_command() const {
  return _pending_command;
}

const int *Process::get_stdout_pipe() const { return _stdout_pipe; }

const int *Process::get_stderr_pipe() const { return _stderr_pipe; }

void Process::set_num_retries(size_t num_retries) {
  _num_retries = num_retries;
}

void Process::set_state(State state) { _state = state; }

void Process::set_previous_state(State state) { _previous_state = state; }

void Process::set_pending_command(Command pending_command) {
  _pending_command = pending_command;
}

void Process::setup_env() const {
  for (std::pair<std::string, std::string> env : _process_config->env) {
    setenv(env.first.c_str(), env.second.c_str(), 1);
  }
}

void Process::setup_workingdir() const {
  if (_process_config->workingdir.empty()) {
    return;
  }
  if (chdir(_process_config->workingdir.c_str()) == -1) {
    throw std::runtime_error(std::string("chdir:") + strerror(errno));
  }
}

void Process::setup_outputs() {
  redirect_output(_stdout_pipe[PIPE_WRITE], STDOUT_FILENO);
  redirect_output(_stderr_pipe[PIPE_WRITE], STDERR_FILENO);
}

static void redirect_output(int new_fd, int current_fd) {
  if (dup2(new_fd, current_fd) == -1) {
    throw std::runtime_error(std::string("dup2:") + strerror(errno));
  }
  // close(new_fd);
}

std::ostream &operator<<(std::ostream &os, const Process::State &state) {
  switch (state) {
  case Process::State::Waiting:
    os << "Waiting";
    break;
  case Process::State::Starting:
    os << "Starting";
    break;
  case Process::State::Running:
    os << "Running";
    break;
  case Process::State::Exiting:
    os << "Exiting";
    break;
  case Process::State::Stopped:
    os << "Stopped";
    break;
  }
  return os;
}
