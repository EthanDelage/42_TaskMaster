#include "server/Process.hpp"

#include "common/socket/Socket.hpp"
#include "server/ConfigParser.hpp"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <signal.h>
extern "C" {
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
}

static void redirect_output(int pipe_fd, int output_fd);

extern char **environ; // envp

Process::Process(std::shared_ptr<const process_config_t> process_config)
    : _process_config(process_config),
      _pid(-1),
      _num_retries(0),
      _state(State::Waiting),
      _previous_state(State::Waiting),
      _pending_command(Command::None),
      _stdout_pipe{-1, -1},
      _stderr_pipe{-1, -1},
      _stdout_fd(-1),
      _stderr_fd(-1) {
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
  std::cout << "Process " << _process_config->name << " created" << std::endl;
}

Process::~Process() {
  close(_stdout_pipe[PIPE_READ]);
  close(_stderr_pipe[PIPE_READ]);
  close(_stdout_fd);
  close(_stderr_fd);
}

int Process::start() {
  std::cout << "[Taskmaster] Starting " << _process_config->name << " ..."
            << std::endl;
  if (pipe(_stdout_pipe) == -1) {
    throw std::runtime_error("Error: Process() failed to create stdout pipe");
  }
  if (pipe(_stderr_pipe) == -1) {
    throw std::runtime_error("Error: Process() failed to create stderr pipe");
  }
  _pid = fork();
  if (_pid == -1) {
    perror("fork");
    return -1;
  }
  if (_pid > 0) {
    // parent process
    close(_stdout_pipe[PIPE_WRITE]);
    _stdout_pipe[PIPE_WRITE] = -1;
    close(_stderr_pipe[PIPE_WRITE]);
    _stderr_pipe[PIPE_WRITE] = -1;
    _start_timestamp = std::chrono::steady_clock::now();
    return 0;
  }
  close(_stdout_pipe[PIPE_READ]);
  close(_stderr_pipe[PIPE_READ]);
  close(_stdout_fd);
  close(_stderr_fd);
  setup();
  execve(_process_config->cmd_path.c_str(), _process_config->cmd->we_wordv,
         environ);
  std::exit(errno);
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

void Process::read_stdout() {
  forward_output(_stdout_pipe[PIPE_READ], _stdout_fd);
}

void Process::read_stderr() {
  forward_output(_stderr_pipe[PIPE_READ], _stderr_fd);
}

void Process::attach_client(int fd) {
  if (std::find(_attached_client.begin(), _attached_client.end(), fd) !=
      _attached_client.end()) {
    // TODO: add log
  } else {
    _attached_client.push_back(fd);
  }
}

void Process::detach_client(int fd) {
  auto client_it =
      std::find(_attached_client.begin(), _attached_client.end(), fd);
  if (client_it == _attached_client.end()) {
    // TODO: add log
  } else {
    _attached_client.erase(client_it);
  }
}

void Process::close_outputs() {
  close(_stdout_pipe[PIPE_READ]);
  _stdout_pipe[PIPE_READ] = -1;
  close(_stderr_pipe[PIPE_READ]);
  _stderr_pipe[PIPE_READ] = -1;
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

const process_config_t &Process::get_process_config() {
  return *_process_config;
}

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

void Process::setup() {
  setup_env();
  setup_workingdir();
  setup_outputs();
  setup_umask();
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

void Process::setup_umask() const { umask(_process_config->umask); }

void Process::setup_outputs() {
  redirect_output(_stdout_pipe[PIPE_WRITE], STDOUT_FILENO);
  redirect_output(_stderr_pipe[PIPE_WRITE], STDERR_FILENO);
}

/**
 * @brief Read data from a pipe and forward it to the main output descriptor
 *        as well as all attached client sockets.
 *
 * @param read_fd   File descriptor from which to read (pipe read end).
 * @param output_fd File descriptor to forward the data to.
 *
 * @note If the read operation fails, the function prints an error using
 * perror() and returns without attempting to forward any data.
 */
void Process::forward_output(int read_fd, int output_fd) {
  char buffer[SOCKET_BUFFER_SIZE];
  ssize_t ret;

  ret = Socket::read(read_fd, buffer, SOCKET_BUFFER_SIZE);
  if (ret == -1) {
    perror("read");
    return;
  }
  Socket::write(output_fd, buffer, ret);
  for (auto client : _attached_client) {
    Socket::write(client, buffer, ret);
  }
}

static void redirect_output(int pipe_fd, int output_fd) {
  if (dup2(pipe_fd, output_fd) == -1) {
    throw std::runtime_error(std::string("dup2:") + strerror(errno));
  }
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
