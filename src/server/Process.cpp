#include "server/Process.hpp"

#include "common/utils.hpp"
#include <chrono>
#include <cstring>
#include <iostream>
#include <signal.h>
extern "C" {
#include <fcntl.h>
#include <sys/wait.h>
}

static void redirect_output(std::string path, int current_output);

extern char **environ; // envp

Process::Process(ProgramConfig &program_config)
    : _program_config(std::move(program_config)),
      _cmd_path(get_cmd_path(_program_config.get_cmd()[0])),
      _pid(-1),
      _startretries(0) {}

int Process::start() {
  std::cout << "[Taskmaster] Starting " << _program_config.get_name() << " ..."
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
  setup_outputs();
  setup_workingdir();
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

std::chrono::steady_clock::time_point Process::get_start_time() const {
  return _start_time;
}

unsigned long Process::get_startretries() const { return _startretries; }

void Process::set_startretries(unsigned long startretries) {
  _startretries = startretries;
}

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

void Process::setup_outputs() const {
  redirect_output(_program_config.get_stdout(), STDOUT_FILENO);
  redirect_output(_program_config.get_stderr(), STDERR_FILENO);
}

void Process::setup_workingdir() const {
  if (_program_config.get_workingdir().empty()) {
    return;
  }
  if (chdir(_program_config.get_workingdir().c_str()) == -1) {
    throw std::runtime_error(std::string("chdir") + strerror(errno));
  }
}

static void redirect_output(std::string path, int current_output) {
  int new_output;

  new_output = path.empty()
                   ? open("/dev/null", O_WRONLY)
                   : open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (new_output == -1) {
    throw std::runtime_error(std::string("open") + strerror(errno));
  }
  if (dup2(new_output, current_output) == -1) {
    throw std::runtime_error(std::string("dup2") + strerror(errno));
  }
  close(new_output);
}
