#include "server/Taskmaster.hpp"
#include "server/ConfigParser.hpp"
#include "server/PollFds.hpp"
#include "server/Process.hpp"
#include "server/TaskManager.hpp"

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

Taskmaster::Taskmaster(const ConfigParser &config)
    : _config(config),
      _command_manager(get_commands_callback()),
      _server_socket(SOCKET_PATH_NAME) {
  std::vector<process_config_s> program_configs = config.parse();

  if (pipe(_wake_up_pipe) == -1) {
    throw std::runtime_error(
        "Error: Taskmaster() failed to create wake_up pipe");
  }
  _poll_fds.add_poll_fd({_server_socket.get_fd(), POLLIN, 0},
                        {PollFds::FdType::Server});
  _poll_fds.add_poll_fd({_wake_up_pipe[PIPE_READ], POLLIN, 0},
                        {PollFds::FdType::WakeUp});
  // TODO: pass pipe write to task manager
  init_process_pool(program_configs);
}

void Taskmaster::loop() {
  TaskManager task_manager(_process_pool, _process_pool_mutex, _poll_fds,
                           _wake_up_pipe[PIPE_WRITE]);

  set_sighup_handler();
  if (_server_socket.listen(BACKLOG) == -1) {
    return;
  }
  while (true) {
    PollFds::snapshot_t poll_fds_snapshot = _poll_fds.get_snapshot();
    int result = poll(poll_fds_snapshot.poll_fds.data(),
                      poll_fds_snapshot.poll_fds.size(), -1);
    if (result == -1) {
      if (errno != EINTR) {
        throw std::runtime_error("poll()");
      }
      continue;
    }
    handle_poll_fds(poll_fds_snapshot);
    if (sighup_received_g) {
      // TODO: reload the config and add log
      for (auto &client_session : _client_sessions) {
        if (client_session.get_reload_request()) {
          // TODO: update the reload response message
          client_session.send_response("successful reload");
          client_session.set_reload_request(false);
        }
      }
      sighup_received_g = 0;
    }
  }
}

void Taskmaster::init_process_pool(
    std::vector<process_config_s> &programs_configs) {
  for (auto &program_config : programs_configs) {
    std::vector<Process> processes;
    std::shared_ptr<process_config_s> shared_program_config;
    shared_program_config =
        std::make_shared<process_config_s>(std::move(program_config));
    for (size_t i = 0; i < shared_program_config->numprocs; ++i) {
      processes.emplace_back(shared_program_config);
    }
    _process_pool.insert({shared_program_config->name, std::move(processes)});
  }
}

void Taskmaster::handle_poll_fds(PollFds::snapshot_t poll_fds_snapshot) {
  for (size_t index = 1; index < poll_fds_snapshot.poll_fds.size(); index++) {
    const pollfd poll_fd = poll_fds_snapshot.poll_fds[index];
    const PollFds::metadata_t fd_metadata = poll_fds_snapshot.metadata[index];

    if (poll_fd.revents != 0) {
      if (fd_metadata.type == PollFds::FdType::Client) {
        handle_client_command(poll_fd);
      } else if (fd_metadata.type == PollFds::FdType::Process) {
        if ((poll_fd.revents & POLLNVAL) != 0) {
          _poll_fds.remove_poll_fd(poll_fd.fd); // TODO: Is this really useful ?
        } else {
          read_process_output(poll_fd.fd);
        }
      } else if (fd_metadata.type == PollFds::FdType::WakeUp) {
        handle_wake_up(poll_fd);
      }
    }
  }
  handle_connection(poll_fds_snapshot);
}

void Taskmaster::handle_client_command(const pollfd &poll_fd) {
  const auto it = get_client_session_from_fd(poll_fd.fd);
  std::string cmd_line;

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

void Taskmaster::handle_connection(PollFds::snapshot_t poll_fds_snapshot) {
  if (poll_fds_snapshot.poll_fds[0].revents & POLLIN) {
    int client_fd = _server_socket.accept_client();
    if (client_fd == -1) {
      // TODO: log handle_connection() failed
      return;
    }
    // TODO: add log
    std::lock_guard lock(_poll_fds.get_mutex());
    _poll_fds.add_poll_fd({client_fd, POLLIN, 0}, {PollFds::FdType::Client});
    _client_sessions.emplace_back(client_fd);
  }
}

void Taskmaster::handle_wake_up(const pollfd &poll_fd) {
  char buffer[SOCKET_BUFFER_SIZE];
  Socket::read(poll_fd.fd, buffer, SOCKET_BUFFER_SIZE);
}

void Taskmaster::read_process_output(int fd) {
  for (auto &[name, processes] : _process_pool) {
    for (auto &process : processes) {
      if (fd == process.get_stdout_pipe()[PIPE_READ]) {
        process.read_stdout();
      } else if (fd == process.get_stderr_pipe()[PIPE_READ]) {
        process.read_stderr();
      }
    }
  }
}

void Taskmaster::disconnect_client(int fd) {
  // TODO: add log
  remove_client_session(fd);
  std::lock_guard lock(_poll_fds.get_mutex());
  _poll_fds.remove_poll_fd(fd);
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
  _current_client->set_reload_request(true);
}

void Taskmaster::quit(const std::vector<std::string> &) {}

void Taskmaster::help(const std::vector<std::string> &) {
  // No server-side implementation
}

void Taskmaster::attach(const std::vector<std::string> &args) {
  std::lock_guard<std::mutex> lock(_process_pool_mutex);
  auto process_group = _process_pool.find(args[1]);
  if (process_group == _process_pool.end()) {
    // TODO: add log
    // TODO: send response to the client
    std::cout << "process group not found" << std::endl;
    return;
  }
  for (auto &process : process_group->second) {
    process.attach_client(_current_client->get_fd());
  }
}

void Taskmaster::detach(const std::vector<std::string> &args) {
  std::lock_guard<std::mutex> lock(_process_pool_mutex);
  auto process_group = _process_pool.find(args[1]);
  if (process_group == _process_pool.end()) {
    // TODO: add log
    // TODO: send response to the client
    return;
  }
  for (auto &process : process_group->second) {
    process.detach_client(_current_client->get_fd());
  }
  _current_client->send_response("Successfully detached\n");
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
      {CMD_HELP_STR, nullptr},
      {CMD_ATTACH_STR,
       [this](const std::vector<std::string> &args) { attach(args); }},
      {CMD_DETACH_STR,
       [this](const std::vector<std::string> &args) { detach(args); }},
  };
}

static void sighup_handler(int) { sighup_received_g = 1; }
