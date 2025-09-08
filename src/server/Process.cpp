#include "server/Process.hpp"

#include "common/utils.hpp"
#include <sys/wait.h>
#include <unistd.h>

extern char **environ; // envp

Process::Process(ProgramConfig &program_config)
    : _program_config(std::move(program_config)),
      _cmd_path(get_cmd_path(_program_config.get_cmd()[0])), _pid(-1),
      _state(State::IDLE) {}

int Process::start() {
  _pid = fork();
  if (_pid == -1) {
    perror("fork");
    return -1;
  }
  if (_pid > 0) {
    return 0;
  }
  if (execve(_cmd_path.c_str(), _program_config.get_cmd(), environ) == -1) {
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

ProgramConfig &Process::get_program_config() { return _program_config; }

State Process::get_state() const { return _state; }

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
