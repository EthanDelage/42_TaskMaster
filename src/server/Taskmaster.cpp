#include "server/Taskmaster.hpp"
#include "server/Process.hpp"
#include "server/config/ProgramConfig.hpp"

#include <csignal>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>

static void sighup_handler(int);

volatile sig_atomic_t sighup_received_g = 0;

Taskmaster::Taskmaster(const Config &config)
    : _config(config),
      _command_manager(get_commands_callback()),
      _server_socket(SOCKET_PATH_NAME) {
  std::vector<ProgramConfig> program_configs = config.parse();

  add_poll_fd({_server_socket.get_fd(), POLLIN, 0}, {FdType::Server});
  init_process_pool(program_configs);
}

void Taskmaster::loop() {
  int result;

  // TODO: Start TaskManager thread
  set_sighup_handler();
  if (_server_socket.listen(BACKLOG) == -1) {
    return;
  }
  while (true) {
    result = poll(_poll_fds.data(), _poll_fds.size(), -1);
    if (result == -1) {
      if (errno != EINTR) {
        throw std::runtime_error("poll()");
      }
      continue;
    }
    handle_poll_fds();
    if (sighup_received_g) {
      // TODO: reload the config
    }
  }
}

void Taskmaster::init_process_pool(std::vector<ProgramConfig>& programs_configs) {
  for (auto &program_config : programs_configs) {
    std::vector<Process> processes;
    std::shared_ptr<ProgramConfig> shared_program_config;
    shared_program_config = std::make_shared<ProgramConfig>(std::move(program_config));
    for (size_t i = 0; i < program_config.get_numprocs(); ++i) {
      processes.emplace_back(shared_program_config);
      add_poll_fd({processes[i].get_stdout_pipe()[PIPE_READ], POLLIN, 0}, {FdType::Process});
      add_poll_fd({processes[i].get_stderr_pipe()[PIPE_READ], POLLIN, 0}, {FdType::Process});
    }
    _process_pool.insert({ program_config.get_name(), processes });
  }
}

void Taskmaster::handle_poll_fds() {
  for (size_t index = 1; index < _poll_fds.size(); index++) {
    const pollfd *poll_fd = &_poll_fds[index];
    const poll_fd_metadata_t *fd_metadata = &_poll_fds_metadata[index];

    if (poll_fd->revents != 0) {
      if (fd_metadata->type == FdType::Client) {
        handle_client_command();
      } else if (fd_metadata->type == FdType::Process) {
        read_process_output(poll_fd->fd);
      }
    }
  }
  handle_connection();
}

void Taskmaster::handle_client_command() {
  // TODO: waiting for ClientSession class
}

void Taskmaster::read_process_output(int fd) {
  (void)fd; // TODO: remove this
  for (auto &[name, processes] : _process_pool) {
    for (auto &process : processes) {
      (void)process; // TODO: remove this
      if (false) {
        // TODO: change condition to (fd == process.get_stdout_pipe()[read])
        // TODO: process.read_stdout()
        return;
      } else if (false) {
        // TODO: change condition to (fd == process.get_stderr_pipe()[read])
        // TODO: process.read_stderr()
        return;
      }
    }
  }
}

void Taskmaster::handle_connection() {
  if (_poll_fds[0].revents & POLLIN) {
    int client_fd = _server_socket.accept_client();
    if (client_fd == -1) {
      // TODO: log handle_connection() failed
      return;
    }
    add_poll_fd({client_fd, POLLIN, 0}, {FdType::Client});
  }
}

void Taskmaster::disconnect_client(int fd) {
  size_t index;
  auto it = std::find_if(_poll_fds.begin(), _poll_fds.end(),
                         [fd](pollfd poll_fd) { return fd == poll_fd.fd; });
  if (it == _poll_fds.end()) {
    throw std::invalid_argument("disconnect_client(): invalid fd");
  }
  index = std::distance(_poll_fds.begin(), it);
  _poll_fds.erase(it);
  _poll_fds_metadata.erase(_poll_fds_metadata.begin() + index);
  // TODO: remove associated ClientSession
}

void Taskmaster::add_poll_fd(pollfd fd, poll_fd_metadata_t metadata) {
  _poll_fds.emplace_back(fd);
  _poll_fds_metadata.emplace_back(metadata);
}

void Taskmaster::remove_poll_fd(const int fd) {
  long index;
  auto it = _poll_fds.begin();

  while (it != _poll_fds.end() && it->fd != fd) {
    ++it;
  }
  if (it == _poll_fds.end()) {
    throw std::invalid_argument("remove_poll_fd(): invalid fd");
  }
  index = std::distance(_poll_fds.begin(), it);
  _poll_fds.erase(it);
  _poll_fds_metadata.erase(_poll_fds_metadata.begin() + index);
}

void Taskmaster::set_sighup_handler() {
  struct sigaction sa = {};
  sa.sa_handler = sighup_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (sigaction(SIGHUP, &sa, nullptr) == -1) {
    perror("sigaction");
  }
}

void Taskmaster::status(const std::vector<std::string> &) {}

void Taskmaster::start(const std::vector<std::string> &args) { (void)args; }

void Taskmaster::stop(const std::vector<std::string> &args) { (void)args; }

void Taskmaster::restart(const std::vector<std::string> &args) { (void)args; }

void Taskmaster::reload(const std::vector<std::string> &) {}

void Taskmaster::quit(const std::vector<std::string> &) {}

void Taskmaster::help(const std::vector<std::string> &) {
  // No server-side implementation
}

std::unordered_map<std::string, cmd_callback_t>
Taskmaster::get_commands_callback() {
  return {
      {CMD_STATUS_STR,
       [this](const std::vector<std::string> &args) { status(args); }},
      {CMD_START_STR,
       [this](const std::vector<std::string> &args) { start(args); }},
      {CMD_STOP_STR,
       [this](const std::vector<std::string> &args) { stop(args); }},
      {CMD_RESTART_STR,
       [this](const std::vector<std::string> &args) { restart(args); }},
      {CMD_RELOAD_STR,
       [this](const std::vector<std::string> &args) { reload(args); }},
      {CMD_QUIT_STR,
       [this](const std::vector<std::string> &args) { quit(args); }},
      {CMD_EXIT_STR,
       [this](const std::vector<std::string> &args) { quit(args); }},
      {CMD_HELP_STR,
       [this](const std::vector<std::string> &args) { help(args); }},
  };
}

static void sighup_handler(int) { sighup_received_g = 1; }
