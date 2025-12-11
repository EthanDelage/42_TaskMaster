#include "server/Process.hpp"

#include "common/Logger.hpp"
#include "common/socket/Socket.hpp"
#include "server/ConfigParser.hpp"
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
extern "C" {
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
}

static void redirect_output(int pipe_fd, int output_fd);

extern char **environ; // envp

Process::Process(std::shared_ptr<const process_config_t> process_config,
                 int stdout_fd, int stderr_fd)
    : _process_config(process_config),
      _pid(-1),
      _num_retries(0),
      _state(State::Waiting),
      _previous_state(State::Waiting),
      _status{.running = false, .killed = false, .exitstatus = -1},
      _pending_command(Command::None),
      _stdout_pipe{-1, -1},
      _stderr_pipe{-1, -1},
      _stdout_fd(stdout_fd),
      _stderr_fd(stderr_fd) {}

void Process::start() {
  Logger::get_instance().info(str() + ": Starting...");
  if (pipe(_stdout_pipe) == -1) {
    throw std::runtime_error("Error: Process() failed to create stdout pipe");
  }
  if (pipe(_stderr_pipe) == -1) {
    throw std::runtime_error("Error: Process() failed to create stderr pipe");
  }
  _pid = fork();
  if (_pid == -1) {
    throw std::runtime_error(std::string("fork: ") + strerror(errno));
  }
  if (_pid > 0) {
    // parent process
    close(_stdout_pipe[PIPE_WRITE]);
    _stdout_pipe[PIPE_WRITE] = -1;
    close(_stderr_pipe[PIPE_WRITE]);
    _stderr_pipe[PIPE_WRITE] = -1;
    _start_timestamp = std::chrono::steady_clock::now();
    _status.killed = false;
    Logger::get_instance().info(str() + ": Started");
    return;
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

void Process::stop(const int sig) {
  if (_pid == -1) {
    _status.running = false;
    throw std::runtime_error(
        "Process::stop(): trying to kill process with pid -1");
  }
  if (::kill(_pid, sig) == -1) {
    _pid = -1;
    _status.running = false;
    throw std::runtime_error(std::string("kill: ") + strerror(errno));
  }
  _stop_timestamp = std::chrono::steady_clock::now();
  Logger::get_instance().info(str() + ": Stopped");
}

void Process::kill() {
  _status.killed = true;
  if (_pid == -1) {
    _status.running = false;
    throw std::runtime_error(
        "Process::kill(): trying to kill process with pid -1");
  }
  if (::kill(_pid, SIGKILL) == -1) {
    _pid = -1;
    _status.running = false;
    throw std::runtime_error(std::string("kill: ") + strerror(errno));
  }
  Logger::get_instance().info(str() + ": Killed");
}

void Process::update_status(void) {
  int status;

  pid_t result = waitpid(_pid, &status, WNOHANG);
  if (result == -1) {
    // Here waitpid returned an error, it may be due to
    // this function being called without the process being started.
    _status.running = false;
    _pid = -1;
    throw std::runtime_error(std::string("waitpid: ") + strerror(errno));
  }
  if (result == 0) {
    _status.running = true;
    return;
  }
  _status.running = false;
  _pid = -1;
  _status.exitstatus = WEXITSTATUS(status);
  if (exited_unexpectedly()) {
    Logger::get_instance().info(str() +
                                ": exited with unexpected status code " +
                                std::to_string(_status.exitstatus));
  } else {
    Logger::get_instance().info(str() + ": exited with expected status code " +
                                std::to_string(_status.exitstatus));
  }
}

bool Process::exited_unexpectedly() const {
  return std::find(_process_config->exitcodes.begin(),
                   _process_config->exitcodes.end(),
                   _status.exitstatus) == _process_config->exitcodes.end();
}

/**
 * @brief Return true if the process needs to be autorestarted.
 **/
bool Process::check_autorestart() const {
  AutoRestart autorestart = _process_config->autorestart;
  if (autorestart == AutoRestart::True) {
    return true;
  }
  if (autorestart == AutoRestart::Unexpected) {
    return exited_unexpectedly();
  }
  return false;
}

ssize_t Process::read_stdout() {
  return forward_output(_stdout_pipe[PIPE_READ], _stdout_fd);
}

ssize_t Process::read_stderr() {
  return forward_output(_stderr_pipe[PIPE_READ], _stderr_fd);
}

void Process::attach_client(int fd) {
  if (std::find(_attached_client.begin(), _attached_client.end(), fd) !=
      _attached_client.end()) {
    Logger::get_instance().warn("Client fd=" + std::to_string(fd) +
                                " already attached to `" +
                                _process_config->name + '`');
  } else {
    _attached_client.push_back(fd);
    Logger::get_instance().info("Client fd=" + std::to_string(fd) +
                                " attached to `" + _process_config->name + '`');
  }
}

void Process::detach_client(int fd) {
  auto client_it =
      std::find(_attached_client.begin(), _attached_client.end(), fd);
  if (client_it == _attached_client.end()) {
    Logger::get_instance().warn("Client fd=" + std::to_string(fd) +
                                " not attached to `" + _process_config->name +
                                '`');
  } else {
    _attached_client.erase(client_it);
    Logger::get_instance().info("Client fd=" + std::to_string(fd) +
                                " detached to `" + _process_config->name + '`');
  }
}

void Process::send_message_to_client(const std::string &message) {
  for (auto client_fd : _attached_client) {
    Socket::write(client_fd, message);
  }
}

void Process::close_outputs() {
  close(_stdout_pipe[PIPE_READ]);
  _stdout_pipe[PIPE_READ] = -1;
  close(_stderr_pipe[PIPE_READ]);
  _stderr_pipe[PIPE_READ] = -1;
}

std::string Process::str() const {
  return "proc [" + _process_config->name + "](" + std::to_string(_pid) + ")";
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

const process_config_t &Process::get_process_config() const {
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
 * @return the number of bytes read
 *
 * @note If the read operation fails, the function prints an error using
 * perror() and returns without attempting to forward any data.
 */
ssize_t Process::forward_output(int read_fd, int output_fd) {
  char buffer[SOCKET_BUFFER_SIZE];
  ssize_t ret;

  ret = Socket::read(read_fd, buffer, SOCKET_BUFFER_SIZE);
  if (ret == -1) {
    perror("read");
    return ret;
  }
  Socket::write(output_fd, buffer, ret);
  for (auto client : _attached_client) {
    Socket::write(client, buffer, ret);
  }
  return ret;
}

static void redirect_output(int pipe_fd, int output_fd) {
  if (dup2(pipe_fd, output_fd) == -1) {
    throw std::runtime_error(std::string("dup2:") + strerror(errno));
  }
}

std::ostream &operator<<(std::ostream &os, const Process &process) {
  os << "(" << process.get_pid() << ") - " << process.get_state();
  if (process.get_state() == Process::State::Stopped &&
      process.get_status().exitstatus != -1) {
    if (process.exited_unexpectedly()) {
      os << " - exited unexpectedly";
    }
    if (process.get_status().killed) {
      os << " - killed";
    }
    if (process.get_num_retries() > process.get_process_config().startretries &&
        process.get_process_config().startretries != 0) {
      os << " - aborted";
    }
  }
  return os;
}

std::ostream &operator<<(std::ostream &os, const Process::State &state) {
  os << process_state_str(state);
  return os;
}

std::string process_state_str(const Process::State &state) {
  switch (state) {
  case Process::State::Waiting:
    return "(Waiting)";
  case Process::State::Starting:
    return "(Starting)";
  case Process::State::Running:
    return "(Running)";
  case Process::State::Exiting:
    return "(Exiting)";
  case Process::State::Stopped:
    return "(Stopped)";
  }
  return "(UNDEFINED)";
}
