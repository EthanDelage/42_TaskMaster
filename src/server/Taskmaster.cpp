#include "server/Taskmaster.hpp"
#include "server/Process.hpp"
#include "server/config/ProgramConfig.hpp"

#include <chrono>
#include <csignal>
#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define PIPE_READ 0
#define PIPE_WRITE 1

static bool process_check_restart(Process &process, int status,
                                  unsigned long runtime);
static bool process_check_unexpected(Process &process, int status);
static void sigchld_handler(int);

volatile sig_atomic_t sigchld_received_g = 0;

Taskmaster::Taskmaster(Config config)
    : _command_manager(get_commands_callback()), _socket(SOCKET_PATH_NAME) {
  std::vector<ProgramConfig> program_configs = config.parse();
  struct sigaction sa;

  _poll_fds.push_back({_socket.get_fd(), POLLIN, 0});
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
  sigset_t mask;
  sigset_t origin_mask;
  int result;

  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);
  sigprocmask(SIG_BLOCK, &mask, &origin_mask);

  if (_socket.listen(BACKLOG) == -1) {
    return;
  }
  autostart_processes();
  while (true) {
    result = ppoll(_poll_fds.data(), _poll_fds.size(), NULL, &origin_mask);
    if (result == -1) {
      if (errno == EINTR) {
        reap_processes();
      } else {
        perror(NULL);
        throw std::runtime_error("poll()");
      }
    } else {
      process_poll_fds();
    }
  }
}

int Taskmaster::autostart_processes() {
  for (Process &process : _processes) {
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
  pid_t pid = 0;
  int status;

  while (!_running_processes.empty() &&
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

void Taskmaster::process_poll_fds() {
  auto it = ++_poll_fds.begin(); // ignore first socket (server_socket)
  std::string cmd_line;

  while (it != _poll_fds.end()) {
    if (it->revents == 0) {
      ++it;
    } else if (it->revents & (POLLERR | POLLHUP | POLLRDHUP)) {
      close(it->fd);
      it = _poll_fds.erase(it);
    } else if (it->revents & POLLIN) {
      try {
        cmd_line = read_command(it->fd);
        run_command(it->fd, cmd_line);
      } catch (const std::runtime_error &) {
        close(it->fd);
        it = _poll_fds.erase(it);
        continue;
      }
      ++it;
    }
  }
  handle_connection();
}

void Taskmaster::run_command(const int cmd_requester_fd,
                             const std::string &cmd_line) {
  _cmd_requester_fd = cmd_requester_fd;
  _command_manager.run_command(cmd_line);
}

std::string Taskmaster::read_command(int fd) {
  char buffer[1024];
  std::string buffer_str;
  ssize_t ret;
  size_t endl_pos;

  ret = read(fd, buffer, sizeof(buffer));
  if (ret == -1) {
    throw std::runtime_error("read_command()");
  }
  if (ret == 0) {
    throw std::runtime_error("client disconnected");
  }
  buffer_str = std::string(buffer, ret);
  endl_pos = buffer_str.find('\n');
  if (endl_pos != std::string::npos) {
    buffer_str.erase(endl_pos);
  }
  return buffer_str;
}

void Taskmaster::handle_connection() {
  if (_poll_fds[0].revents & POLLIN) {
    const int client_fd = _socket.accept_client();
    if (client_fd == -1) {
      std::cerr << "[Taskmaster] accept_client() failed" << std::endl;
      return;
    }
    _poll_fds.push_back({client_fd, POLLIN | POLLRDHUP, 0});
  }
}

std::unordered_map<std::string, cmd_callback_t>
Taskmaster::get_commands_callback() {
  return {
      {CMD_STATUS_STR,
       [this](const std::vector<std::string> &args) { callback(args); }},
      {CMD_START_STR,
       [this](const std::vector<std::string> &args) { callback(args); }},
      {CMD_STOP_STR,
       [this](const std::vector<std::string> &args) { callback(args); }},
      {CMD_RESTART_STR,
       [this](const std::vector<std::string> &args) { callback(args); }},
      {CMD_RELOAD_STR,
       [this](const std::vector<std::string> &args) { callback(args); }},
      {CMD_QUIT_STR,
       [this](const std::vector<std::string> &args) { callback(args); }},
      {CMD_EXIT_STR,
       [this](const std::vector<std::string> &args) { callback(args); }},
      {CMD_HELP_STR,
       [this](const std::vector<std::string> &args) { callback(args); }},
  };
}

void Taskmaster::callback(const std::vector<std::string> &args) {
  std::cout << "[Taskmaster] received: " << args[0] << std::endl;
  if (write(_cmd_requester_fd, args[0].c_str(), args[0].length()) == -1) {
    throw std::runtime_error("[Taskmaster] write()");
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

static void sigchld_handler(int) { sigchld_received_g = 1; }
