#include "server/Process.hpp"

#include "common/utils.hpp"
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

Process::Process(std::shared_ptr<ProgramConfig> program_config)
    : _program_config(std::move(program_config)),
      _pid(-1),
      _num_retries(0),
      _state(State::Waiting),
      _requested_command(Command::None),
      _cmd_path(get_cmd_path(_program_config->get_cmd()[0])) {
  if (pipe(_stdout_pipe) == -1 || pipe(_stderr_pipe) == -1) {
    throw std::runtime_error("Error: Process() failed to create pipe");
  }
  std::string stdout_path = _program_config->get_stdout();
  _stdout_fd = stdout_path.empty()
                   ? open("/dev/null", O_WRONLY)
                   : open(stdout_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (_stdout_fd == -1) {
    throw std::runtime_error(std::string("open") + strerror(errno));
  }
  std::string stderr_path = _program_config->get_stderr();
  _stderr_fd = stderr_path.empty()
                   ? open("/dev/null", O_WRONLY)
                   : open(stderr_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (_stderr_fd == -1) {
    throw std::runtime_error(std::string("open") + strerror(errno));
  }
  std::cout << "Process " << _program_config->get_name() << " created" << std::endl;
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
  std::cout << "[Taskmaster] Starting " << _program_config->get_name() << " ..."
            << std::endl;
  _pid = fork();
  if (_pid == -1) {
    perror("fork");
    return -1;
  }
  if (_pid > 0) {
    // parent process
    _start_time = std::chrono::steady_clock::now();
    return 0;
  }
  // child process
  setup_env();
  setup_outputs();
  setup_workingdir();
  if (execve(_cmd_path.c_str(), _program_config->get_cmd(), environ) == -1) {
    perror("execve");
    return -1;
  }
  return 0;
}

int Process::stop(const int sig) {
  if (kill(_pid, sig) == -1) {
    perror("kill");
    return -1;
  }
  if (wait(nullptr) == -1) {
    perror("wait");
    return -1;
  }
  _pid = -1;
  return 0;
}

int Process::restart(int sig) {
  if (stop(sig) == -1) {
    return -1;
  }
  return start();
}

pid_t Process::get_pid() const { return _pid; }

ProgramConfig &Process::get_program_config() { return *_program_config; }

std::chrono::steady_clock::time_point Process::get_start_time() const {
  return _start_time;
}

size_t Process::get_num_retries() const { return _num_retries; }

Process::State Process::get_state() const { return _state; }

const int *Process::get_stdout_pipe() const { return _stdout_pipe; }

const int *Process::get_stderr_pipe() const { return _stderr_pipe; }

void Process::set_num_retries(size_t num_retries) { _num_retries = num_retries; }

void Process::set_state(State state) { _state = state; }

void Process::set_requested_command(Command requested_command) { _requested_command = requested_command; }

std::string Process::get_cmd_path(const std::string &cmd) {
  if (cmd.find('/') != std::string::npos) {
    return cmd;
  }

  char *env_path = std::getenv("PATH");
  if (env_path == nullptr) {
    throw std::runtime_error("Error: please define the PATH env variable");
  }
  std::vector<std::string> path_list = split(env_path, ':');

  for (const auto &path : path_list) {
    std::string cmd_path = path + '/' + cmd;
    if (access(cmd_path.c_str(), X_OK) == 0) {
      return cmd_path;
    }
  }
  throw std::runtime_error("Error: command not found: " + cmd);
}

void Process::setup_env() const {
  for (std::pair<std::string, std::string> env : _program_config->get_env()) {
    setenv(env.first.c_str(), env.second.c_str(), 1);
  }
}

void Process::setup_workingdir() const {
  if (_program_config->get_workingdir().empty()) {
    return;
  }
  if (chdir(_program_config->get_workingdir().c_str()) == -1) {
    throw std::runtime_error(std::string("chdir") + strerror(errno));
  }
}

void Process::setup_outputs() {
  redirect_output(_stdout_pipe[PIPE_WRITE], STDOUT_FILENO);
  redirect_output(_stderr_pipe[PIPE_WRITE], STDERR_FILENO);
}

static void redirect_output(int new_fd, int current_fd) {
  if (dup2(new_fd, current_fd) == -1) {
    throw std::runtime_error(std::string("dup2") + strerror(errno));
  }
  close(new_fd);
}
