#include "server/Taskmaster.hpp"
#include "server/ConfigParser.hpp"
#include "server/PollFds.hpp"
#include "server/Process.hpp"
#include "server/TaskManager.hpp"

#include <common/Logger.hpp>
#include <csignal>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>

static bool compare_config(const process_config_t &left,
                           const process_config_t &right);
static void sighup_handler(int);

volatile sig_atomic_t sighup_received_g = 0;

Taskmaster::Taskmaster(const ConfigParser &config)
    : _config(config),
      _command_manager(get_commands_callback()),
      _process_pool(config.parse()),
      _server_socket(SOCKET_PATH_NAME),
      _task_manager(_process_pool, _poll_fds),
      _running(true) {
  if (pipe(_wake_up_pipe) == -1) {
    throw std::runtime_error(
        "Error: Taskmaster() failed to create wake_up pipe");
  }
  _task_manager.set_wake_up_fd(_wake_up_pipe[PIPE_WRITE]);
  _poll_fds.add_poll_fd({_server_socket.get_fd(), POLLIN, 0},
                        {PollFds::FdType::Server});
  _poll_fds.add_poll_fd({_wake_up_pipe[PIPE_READ], POLLIN, 0},
                        {PollFds::FdType::WakeUp});
}

void Taskmaster::loop() {
  _task_manager.start();
  set_sighup_handler();
  if (_server_socket.listen(BACKLOG) == -1) {
    return;
  }
  while (_running) {
    PollFds::snapshot_t poll_fds_snapshot = _poll_fds.get_snapshot();
    int result = poll(poll_fds_snapshot.poll_fds.data(),
                      poll_fds_snapshot.poll_fds.size(), -1);
    if (result == -1) {
      if (errno != EINTR) {
        throw std::runtime_error("poll()");
      }
      continue;
    }
    if (!_task_manager.is_thread_alive()) {
      return;
    }
    handle_poll_fds(poll_fds_snapshot);
    if (sighup_received_g) {
      reload_config();
      for (auto &client_session : _client_sessions) {
        if (client_session.get_reload_request()) {
          // TODO: update the reload response message
          client_session.send_response("successful reload\n");
          client_session.set_reload_request(false);
        }
      }
      sighup_received_g = 0;
    }
  }
}

void Taskmaster::handle_poll_fds(const PollFds::snapshot_t &poll_fds_snapshot) {
  for (size_t index = 0; index < poll_fds_snapshot.poll_fds.size(); index++) {
    const pollfd poll_fd = poll_fds_snapshot.poll_fds[index];
    const auto [fd_type] = poll_fds_snapshot.metadata[index];

    if (poll_fd.revents != 0) {
      Logger::get_instance().debug(
          "fd=" + std::to_string(poll_fd.fd) +
          " revents=" + std::to_string(poll_fd.revents));
      if ((poll_fd.revents & POLLNVAL) != 0) {
        _poll_fds.remove_poll_fd(poll_fd.fd);
        continue;
      }
      switch (fd_type) {
      case PollFds::FdType::Process:
        handle_process_output(poll_fd.fd);
        break;
      case PollFds::FdType::Client:
        handle_client_command(poll_fd);
        break;
      case PollFds::FdType::WakeUp:
        handle_wake_up(poll_fd.fd);
        break;
      case PollFds::FdType::Server:
        handle_connection();
        break;
      }
    }
  }
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

void Taskmaster::handle_connection() {
  int client_fd = _server_socket.accept_client();
  if (client_fd == -1) {
    return;
  }
  _poll_fds.add_poll_fd({client_fd, POLLIN, 0}, {PollFds::FdType::Client});
  _client_sessions.emplace_back(client_fd);
}

void Taskmaster::handle_wake_up(int fd) {
  char buffer[SOCKET_BUFFER_SIZE];
  Socket::read(fd, buffer, SOCKET_BUFFER_SIZE);
}

void Taskmaster::handle_process_output(int fd) {
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
void Taskmaster::reload_config() {
  ProcessPool new_pool(_config.parse());

  std::lock_guard lock(_process_pool.get_mutex());
  Logger::get_instance().info("Reloading config...");
  for (auto it = new_pool.begin(); it != new_pool.end();) {
    auto old_it = _process_pool.find(it->first);
    if (old_it != _process_pool.end()) {
      if (compare_config(old_it->second.get_process_config(),
                         it->second.get_process_config())) {
        Logger::get_instance().info("No need to reload " + it->second.str());
        it = new_pool.erase(it);
        new_pool.move_from(_process_pool, old_it->first);
        continue;
      }
      Logger::get_instance().info("Reloading " + it->second.str());
    }
    ++it;
  }
  for (auto &it : _process_pool) {
    it.second.stop(SIGKILL);
  }
  Logger::get_instance().info("Config successfully reloaded");
  _process_pool = std::move(new_pool);
}

void Taskmaster::disconnect_client(int fd) {
  Logger::get_instance().info("Client fd=" + std::to_string(fd) +
                              " disconnected");
  remove_client_session(fd);
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

void Taskmaster::status(const std::vector<std::string> &) {
  std::ostringstream oss;

  oss << _process_pool;
  _current_client->send_response(oss.str());
}

void Taskmaster::start(const std::vector<std::string> &args) {
  request_command(args, Process::Command::Start);
}

void Taskmaster::stop(const std::vector<std::string> &args) {
  request_command(args, Process::Command::Stop);
}

void Taskmaster::restart(const std::vector<std::string> &args) {
  request_command(args, Process::Command::Restart);
}

void Taskmaster::reload(const std::vector<std::string> &) {
  sighup_received_g = 1;
  _current_client->set_reload_request(true);
}

void Taskmaster::quit(const std::vector<std::string> &) {
  _running = false;
  _task_manager.stop();
  _current_client->send_response("Quitting taskmaster...\n");
}

void Taskmaster::help(const std::vector<std::string> &) {
  // No server-side implementation
}

void Taskmaster::attach(const std::vector<std::string> &args) {
  std::lock_guard<std::mutex> lock(_process_pool.get_mutex());
  auto process_group = _process_pool.find(args[1]);
  if (process_group == _process_pool.end()) {
    Logger::get_instance().warn(
        "Client fd=" + std::to_string(_current_client->get_fd()) +
        " no such process named `" + args[1] + "`");
    _current_client->send_response("No such process named `" + args[1] + "`\n");
    return;
  }
  for (auto &process : process_group->second) {
    process.attach_client(_current_client->get_fd());
  }
}

void Taskmaster::detach(const std::vector<std::string> &args) {
  std::lock_guard<std::mutex> lock(_process_pool.get_mutex());
  auto process_group = _process_pool.find(args[1]);
  if (process_group == _process_pool.end()) {
    Logger::get_instance().warn(
        "Client fd=" + std::to_string(_current_client->get_fd()) +
        " no such process named `" + args[1] + "`");
    _current_client->send_response("No such process named `" + args[1] + "`\n");
    return;
  }
  for (auto &process : process_group->second) {
    process.detach_client(_current_client->get_fd());
  }
  _current_client->send_response("Successfully detached\n");
}

void Taskmaster::request_command(const std::vector<std::string> &args,
                                 Process::Command command) {
  std::lock_guard lock(_process_pool.get_mutex());
  auto process_pool_item = _process_pool.find(args[1]);
  if (process_pool_item == _process_pool.end()) {
    Logger::get_instance().warn(
        "Client fd=" + std::to_string(_current_client->get_fd()) +
        " no such process named `" + args[1] + "`");
    _current_client->send_response("Process named `" + args[1] +
                                   "` not exist\n");
    return;
  }
  for (Process &process : process_pool_item->second) {
    process.set_pending_command(command);
  }
  _current_client->send_response("Command issued successfully\n");
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

static bool compare_config(const process_config_t &left,
                           const process_config_t &right) {
  if (left.cmd->we_wordc != right.cmd->we_wordc) {
    return false;
  }
  for (size_t i = 0; i < left.cmd->we_wordc; i++) {
    if (strcmp(left.cmd->we_wordv[i], right.cmd->we_wordv[i]) != 0) {
      return false;
    }
  }
  return left.name == right.name && left.cmd_path == right.cmd_path &&
         left.workingdir == right.workingdir && left.stdout == right.stdout &&
         left.stderr == right.stderr && left.stopsignal == right.stopsignal &&
         left.numprocs == right.numprocs && left.starttime == right.starttime &&
         left.stoptime == right.stoptime && left.umask == right.umask &&
         left.autostart == right.autostart &&
         left.autorestart == right.autorestart && left.env == right.env &&
         left.exitcodes == right.exitcodes;
}

static void sighup_handler(int) { sighup_received_g = 1; }
