#include "server/Taskmaster.hpp"

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
extern "C" {
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>
}

#define PIPE_READ 0
#define PIPE_WRITE 1

static void sigchld_handler(int);

volatile sig_atomic_t sigchld_received_g = 0;
int sigchld_pipe_g[2]; // This pipe needs to be deleted but for now it is used
                       // to poll until UNIX sockets gets implemented

Taskmaster::Taskmaster(Config config) {
  std::vector<ProgramConfig> program_configs = config.parse();
  struct sigaction sa;

  pipe(sigchld_pipe_g);

  sa.sa_handler = sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    throw std::runtime_error("sigaction()");
  }
  for (auto &program_config : program_configs) {
    _processes.emplace_back(program_config);
  }
}

void Taskmaster::loop() {
  struct pollfd fds[1];
  sigset_t mask;
  sigset_t origin_mask;
  int result;

  fds[0].fd = sigchld_pipe_g[PIPE_READ];
  fds[0].events = POLLIN;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);
  sigprocmask(SIG_BLOCK, &mask, &origin_mask);

  start();
  while (true) {
    std::cout << "Now polling..." << std::endl;
    result = ppoll(fds, 1, NULL, &origin_mask);
    if (result == -1) {
      if (errno == EINTR) {
        reap_processes();
        errno = 0;
      } else {
        perror(NULL);
        throw std::runtime_error("poll()");
      }
    }
  }
}

int Taskmaster::start() {
  for (Process &process : _processes) {
    if (process.get_program_config().get_autostart()) {
      std::cout << "[Taskmaster] Starting "
                << process.get_program_config().get_name() << " ..."
                << std::endl;
      process.start();
      _running_processes.emplace(process.get_pid(), process);
    }
  }
  return 0;
}

void Taskmaster::reap_processes() {
  pid_t pid;
  int status;

  while (_running_processes.size() > 0 &&
         (pid = waitpid(-1, &status, WNOHANG)) > 0) {
    std::cout << "Process " << pid << " exited with status " << status
              << std::endl;
    auto it = _running_processes.find(pid);
    if (it == _running_processes.end()) {
      std::cout << "Process " << pid << " not found in _launched_processes"
                << std::endl;
      // This error should never occur, if it does something went wrong when
      // adding processes to _running_processes
      throw std::runtime_error("trying to reap a process whose pid is not "
                               "found in _running_processes");
    }
    _running_processes.erase(it);
    std::cout << _running_processes.size() << " process currently launched"
              << std::endl;
  }
  if (pid == -1) {
    perror("waitpid()");
    throw std::runtime_error("waitpid()");
  }
}

static void sigchld_handler(int) { sigchld_received_g = 1; }
