#include "server/Taskmaster.hpp"
#include "server/ConfigParser.hpp"
#include "server/Process.hpp"
#include "server/TaskManager.hpp"

#include <csignal>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unordered_map>

static bool compare_config(const process_config_t &left, const process_config_t &right);
static void sighup_handler(int);

volatile sig_atomic_t sighup_received_g = 0;

Taskmaster::Taskmaster(const ConfigParser &config)
    : _config(config),
      _command_manager(get_commands_callback()),
      _server_socket(SOCKET_PATH_NAME),
      _running(true) {
  std::unordered_map<std::string, process_config_t> program_configs = config.parse();

  add_poll_fd({_server_socket.get_fd(), POLLIN, 0}, {FdType::Server});
  init_process_pool(program_configs);
}

void Taskmaster::loop() {
  int result;

  TaskManager task_manager(_process_pool, _process_pool_mutex);
  set_sighup_handler();
  if (_server_socket.listen(BACKLOG) == -1) {
    return;
  }
  while (_running) {
    result = poll(_poll_fds.data(), _poll_fds.size(), -1);
    if (result == -1) {
      if (errno != EINTR) {
        throw std::runtime_error("poll()");
      }
      continue;
    }
    handle_poll_fds();
    if (sighup_received_g) {
      reload_config();
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
  std::unordered_map<std::string, process_config_t> &process_configs) {
  for (auto &[process_name, process_config]: process_configs) {
    insert_process(process_config);
  }
}

void Taskmaster::insert_process(process_config_t &process_config) {
  std::vector<Process> process_group;
  std::shared_ptr<process_config_t> shared_program_config;

  shared_program_config = std::make_shared<process_config_t>(std::move(process_config));
  for (size_t i = 0; i < shared_program_config->numprocs; ++i) {
    process_group.emplace_back(shared_program_config);
  }
  _process_pool.insert({shared_program_config->name, std::move(process_group)});
  for (auto &process : process_group) {
    add_poll_fd({process.get_stdout_pipe()[PIPE_READ], POLLIN, 0},
                {FdType::Process});
    add_poll_fd({process.get_stderr_pipe()[PIPE_READ], POLLIN, 0},
                {FdType::Process});
  }
}

void Taskmaster::remove_process(std::string const & process_name) {
  auto it = _process_pool.find(process_name);

  for (auto &process : it->second) {
    std::cout << "process is not running, killing it..." << std::endl;
    if (process.get_status().running) {
      std::cout << "process is running, killing it..." << std::endl;
      process.stop(SIGKILL);
      process.update_status();
    }
    std::cout << "removing poll fds..." << std::endl;
    remove_poll_fd(process.get_stdout_pipe()[PIPE_READ]);
    remove_poll_fd(process.get_stderr_pipe()[PIPE_READ]);
  }
  _process_pool.erase(it);
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

/*
 * Iterate the new process pool, for each:
 * If the process is already in the old pool but not in the new pool:
 * -> Remove the process
 * If the process is in old and new pool:
 * -> If the process differ:
 *   -> Recreate the process
 *
 */

void Taskmaster::reload_config() {
  std::cout << "Now reloading the config file..." << std::endl;
  std::unordered_map<std::string, process_config_t> new_process_configs;
  std::unordered_map<std::string, std::vector<Process>> new_process_pool;
  std::cout << "Current process pool:" << std::endl;
  for (auto & process : _process_pool) {
    std::cout << "Process name: " << process.first << std::endl;
  }

  std::cout << "Parsing the config file..." << std::endl;
  new_process_configs = _config.parse();
  std::cout << "Locking mutex..." << std::endl;
  std::lock_guard lock(_process_pool_mutex);
  for (auto &[new_process_name, new_process_config]: new_process_configs) {
    std::cout << "Looking at " << new_process_name << ":" << std::endl;
    auto old_process_group = _process_pool.find(new_process_name);
    if (old_process_group == _process_pool.end()) {
      // new process is not found in the pool
      std::cout << "new process is not found in the pool" << std::endl;
      // pool_insert_new_process(new_process_pool, new_process_config);
    } else {
      // new process is already in the pool
      std::cout << "Process is in the pool AND in the new config" << std::endl;
      if (compare_config(old_process_group->second[0].get_process_config(), new_process_config)) {
        std::cout << "Processes differ" << std::endl;
        // old and new processes differ
        std::cout << "removing old process..." << std::endl;
        remove_process(old_process_group->first);
        std::cout << "removing old process..." << std::endl;
        // pool_insert_new_process(new_process_pool, new_process_config);
      }
    }
  }
  // Now the current pool is filled with stale processes, remove them
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

void Taskmaster::status(const std::vector<std::string> &) {
  std::ostringstream oss;

  for (auto const &[process_name, processes] : _process_pool) {
    for (size_t i = 0; i < processes.size(); i++) {
      oss << processes[i] << '[' << i << "] state: " << processes[i].get_state()
          << std::endl;
    }
  }
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
  _current_client->send_response("Quitting taskmaster...\n");
}

void Taskmaster::help(const std::vector<std::string> &) {
  // No server-side implementation
}

void Taskmaster::request_command(const std::vector<std::string> &args,
                                 Process::Command command) {
  for (std::string process_name : args) {
    std::lock_guard lock(_process_pool_mutex);
    auto process_pool_item = _process_pool.find(process_name);
    if (process_pool_item == _process_pool.end()) {
      // TODO the process was not found
      continue;
    }
    for (Process &process : process_pool_item->second) {
      process.set_pending_command(command);
    }
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
      {CMD_HELP_STR,
       [this](const std::vector<std::string> &args) { help(args); }},
  };
}

static bool compare_config(const process_config_t &left, const process_config_t &right) {
  if (memcmp(left.cmd.get(), right.cmd.get(), sizeof(wordexp_t)) != 0) {
    return false;
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
