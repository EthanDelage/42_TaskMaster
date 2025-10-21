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
      // TODO: reload the config and add log
      for (auto client_session : _reload_requesting_clients) {
        // TODO: update the reload response message
        client_session->send_response("successful reload");
      }
      _reload_requesting_clients.clear();
      sighup_received_g = 0;
    }
  }
}

void Taskmaster::init_process_pool(
    std::vector<ProgramConfig> &programs_configs) {
  for (auto &program_config : programs_configs) {
    std::vector<Process> processes;
    std::shared_ptr<ProgramConfig> shared_program_config;
    shared_program_config =
        std::make_shared<ProgramConfig>(std::move(program_config));
    for (size_t i = 0; i < program_config.get_numprocs(); ++i) {
      processes.emplace_back(shared_program_config);
      add_poll_fd({processes[i].get_stdout_pipe()[PIPE_READ], POLLIN, 0},
                  {FdType::Process});
      add_poll_fd({processes[i].get_stderr_pipe()[PIPE_READ], POLLIN, 0},
                  {FdType::Process});
    }
    _process_pool.insert({program_config.get_name(), processes});
  }
}

void Taskmaster::handle_poll_fds() {
  for (size_t index = 1; index < _poll_fds.size(); index++) {
    const pollfd *poll_fd = &_poll_fds[index];
    const poll_fd_metadata_t *fd_metadata = &_poll_fds_metadata[index];

    if (poll_fd->revents != 0) {
      if (fd_metadata->type == FdType::Client) {
        handle_client_command(*poll_fd);
      } else if (fd_metadata->type == FdType::Process) {
        read_process_output(poll_fd->fd);
      }
    }
  }
  handle_connection();
}

void Taskmaster::handle_client_command(const pollfd &poll_fd) {
  const auto it = get_client_session_from_fd(poll_fd.fd);
  std::string cmd_line;
  std::string cmd;

  if (it == _client_sessions.end()) {
    throw std::runtime_error("handle_client_command(): invalid fd");
  }

  if (poll_fd.revents & (POLLERR | POLLHUP)) {
    disconnect_client(poll_fd.fd);
  } else if (poll_fd.revents & POLLIN) {
    try {
      cmd_line = it->recv_command();
    } catch (const std::runtime_error &e) {
      disconnect_client(poll_fd.fd);
      return;
    }
    _current_client = &(*it);
    _command_manager.run_command(cmd_line);
  }
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
    // TODO: add log
    add_poll_fd({client_fd, POLLIN, 0}, {FdType::Client});
    _client_sessions.emplace_back(client_fd);
  }
}

void Taskmaster::disconnect_client(int fd) {
  // TODO: add log
  remove_client_session(fd);
  remove_poll_fd(fd);
}

void Taskmaster::add_poll_fd(pollfd fd, poll_fd_metadata_t metadata) {
  _poll_fds.emplace_back(fd);
  _poll_fds_metadata.emplace_back(metadata);
}

void Taskmaster::remove_poll_fd(const int fd) {
  long index;
  auto it = std::find_if(_poll_fds.begin(), _poll_fds.end(),
                         [fd](pollfd poll_fd) { return fd == poll_fd.fd; });
  if (it == _poll_fds.end()) {
    throw std::invalid_argument("remove_poll_fd(): invalid fd");
  }
  index = std::distance(_poll_fds.begin(), it);
  _poll_fds.erase(it);
  _poll_fds_metadata.erase(_poll_fds_metadata.begin() + index);
}

void Taskmaster::remove_client_session(int fd) {
  auto it = get_client_session_from_fd(fd);

  if (it == _client_sessions.end()) {
    throw std::runtime_error("disconnect_client(): invalid fd");
  }
  _client_sessions.erase(it);
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

void Taskmaster::reload(const std::vector<std::string> &) {
  sighup_received_g = 1;
  _reload_requesting_clients.push_back(_current_client);
}

void Taskmaster::quit(const std::vector<std::string> &) {}

void Taskmaster::help(const std::vector<std::string> &) {
  // No server-side implementation
}

std::vector<ClientSession>::iterator
Taskmaster::get_client_session_from_fd(int fd) {
  return std::find_if(
      _client_sessions.begin(), _client_sessions.end(),
      [fd](const ClientSession &session) { return fd == session.get_fd(); });
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
