#include "server/Taskmaster.hpp"
#include "server/Process.hpp"
#include "server/config/ProgramConfig.hpp"

#include <chrono>
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

static bool process_check_restart(Process &process, int status,
                                  unsigned long runtime);
static bool process_check_unexpected(Process &process, int status);
static void sigchld_handler(int);
static void sighup_handler(int);

volatile sig_atomic_t sigchld_flag_g = 0;
volatile sig_atomic_t sighup_flag_g = 0;
int sigchld_pipe_g[2]; // This pipe needs to be deleted but for now it is used
                       // to poll until UNIX sockets gets implemented

Taskmaster::Taskmaster(Config config) : _config(config){
  std::vector<ProgramConfig> program_configs = _config.parse();
  struct sigaction sa;

  pipe(sigchld_pipe_g);

  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  sa.sa_handler = sigchld_handler;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    throw std::runtime_error("sigaction()");
  }
  sa.sa_handler = sighup_handler;
  if (sigaction(SIGHUP, &sa, NULL) == -1) {
    throw std::runtime_error("sigaction()");
  }
  for (auto &program_config : program_configs) {
    _processes.emplace(program_config.get_name(), program_config);
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

  autostart_processes();
  while (true) {
    result = ppoll(fds, 1, NULL, &origin_mask);
    if (result == -1) {
      if (errno == EINTR) {
        signal_interruption_handler();
      } else {
        perror(NULL);
        throw std::runtime_error("poll()");
      }
    }
  }
}

int Taskmaster::autostart_processes() {
  for (auto & [_, process]: _processes) {
    if (process.get_program_config().get_autostart()) {
      start_process(process);
    }
  }
  return 0;
}

void Taskmaster::start_process(Process &process) {
  process.start();
  _running_processes.emplace(process.get_pid(), process);
}

void Taskmaster::reap_processes() {
  pid_t pid;
  int status;

  while (_running_processes.size() > 0 &&
         (pid = waitpid(-1, &status, WNOHANG)) > 0) {
    status = WEXITSTATUS(status);
    std::cout << "[Taskmaster] Process " << pid << " exited with status "
              << status << std::endl;
    process_termination_handler(pid, status);
  }
  if (pid == -1) {
    perror("waitpid()");
    throw std::runtime_error("waitpid()");
  }
}

void Taskmaster::update_config() {
  std::vector<ProgramConfig> new_program_configs = _config.parse();
  for (auto &new_program_config : new_program_configs) {
    if (_processes.find(new_program_config.get_name()) == _processes.end()) {
    }
  }
}

void Taskmaster::process_termination_handler(pid_t pid, int exitcode) {
  unsigned long runtime;
  auto it = _running_processes.find(pid);
  if (it == _running_processes.end()) {
    std::cerr << "Error: Process " << pid << " not found in _launched_processes"
              << std::endl;
    // This error should never occur, if it does something went wrong when
    // adding/deleting running processes
    throw std::runtime_error("trying to reap a process whose pid is not "
                             "found in _running_processes");
  }
  runtime = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - it->second.get_start_time())
                .count();
  _running_processes.erase(it);
  if (process_check_restart(it->second, exitcode, runtime)) {
    start_process(it->second);
  }
}

void Taskmaster::signal_interruption_handler(void) {
  if (sigchld_flag_g) {
    sigchld_flag_g = 0;
    reap_processes();
  }
  if (sighup_flag_g) {
    sighup_flag_g = 0;
    
  }
}

static bool process_check_restart(Process &process, int exitcode,
                                  unsigned long runtime) {
  if (runtime < process.get_program_config().get_starttime()) {
    // Process is unsuccessfully started
    std::cout << "[Taskmaster] process " << process.get_pid()
              << " unsuccessfully started" << std::endl;
    if (process.get_startretries() >=
        process.get_program_config().get_startretries()) {
      std::cout << "[Taskmaster] process " << process.get_pid() << " aborting"
                << std::endl;
      // Process starting aborted
      process.set_startretries(0);
      return false;
    } else {
      std::cout << "[Taskmaster] process " << process.get_pid() << " retrying"
                << std::endl;
      process.set_startretries(process.get_startretries() + 1);
      return true;
    }
  }
  // Process is successfully started
  switch (process.get_program_config().get_autorestart()) {
  case AutoRestart::False:
    std::cout << "[Taskmaster] process " << process.get_pid()
              << " autorestart: false" << std::endl;
    return false;
  case AutoRestart::True:
    std::cout << "[Taskmaster] process " << process.get_pid()
              << " autorestart: true" << std::endl;
    return true;
  case AutoRestart::Unexpected:
    std::cout << "[Taskmaster] process " << process.get_pid()
              << " autorestart: unexpected" << std::endl;
    return process_check_unexpected(process, exitcode);
  default:
    throw std::runtime_error("process_check_restart(): unexpected value");
  }
}

/*
 * @brief Return true if the status is unexpected, false otherwise
 */
static bool process_check_unexpected(Process &process, int status) {
  for (const auto &elem : process.get_program_config().get_exitcodes()) {
    if (elem == status) {
      return false;
    }
  }
  return true;
}

static void sigchld_handler(int) { sigchld_flag_g = 1; }

static void sighup_handler(int) { sigchld_flag_g = 1; }
