#include "server/Taskmaster.hpp"
#include "server/Process.hpp"
#include "server/config/ProgramConfig.hpp"

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
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
  sigaddset(&mask, SIGHUP);
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

void Taskmaster::autostart_processes() {
  for (auto & [_, process]: _processes) {
    autostart_process(process);
  }
}

void Taskmaster::autostart_process(Process &process) {
  if (process.get_program_config().get_autostart()) {
    start_process(process);
  }
}


void Taskmaster::start_process(Process &process) {
  process.start();
  _running_processes.emplace(process.get_pid(), process);
}

void Taskmaster::stop_process(Process &process) {
  process.stop();
  _running_processes.erase(process.get_pid());
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
  std::unordered_map<std::string, Process> new_processes;

  for (auto &new_program_config : new_program_configs) {
    auto it = _processes.find(new_program_config.get_name());
    if (it == _processes.end()) {
      // New program_config is not found in the current process list
      std::cout << "Process " << new_program_config.get_name() << " not in current list, starting..." << std::endl;
      Process new_process(new_program_config);
      autostart_process(new_process);
      new_processes.insert({new_program_config.get_name(), std::move(new_process)});
    } else if (new_program_config != it->second.get_program_config()) {
      // New program_config is found but differ
      std::cout << "Process " << new_program_config.get_name() << " differ, stopping..." << std::endl;
      stop_process(it->second);
      _processes.erase(new_program_config.get_name());
      Process new_process(new_program_config);
      autostart_process(new_process);
      new_processes.insert({new_program_config.get_name(), std::move(new_process)});
    } else {
      // New program_config does not differ from the old one
      auto node = _processes.extract(new_program_config.get_name());
      new_processes.insert(std::move(node));
      std::cout << "Process " << new_program_config.get_name() << " does not differ, continuing..." << std::endl;
    }
  }
  for (auto &[_, stale_process] : _processes) {
    stop_process(stale_process);
  }
  _processes.clear();
  _processes = std::move(new_processes);
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
    update_config();
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

static void sighup_handler(int) { std::cout << "SIGHUP_HANDLER" << std::endl; sighup_flag_g = 1; }
