#include "client/TaskmasterCtl.hpp"

#include <common/Logger.hpp>
#include <common/utils.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>

volatile sig_atomic_t sigint_received_g = 0;

static void sigint_handler(int);

TaskmasterCtl::TaskmasterCtl(std::string prompt_string)
    : _command_manager(get_commands_callback()),
      _prompt_string(std::move(prompt_string)),
      _is_running(true),
      _socket(SOCKET_PATH_NAME) {
  _usage_max_len = get_usage_max_len();
  _socket.connect();
}

void TaskmasterCtl::loop() {
  std::string input;

  print_header();
  while (_is_running) {
    std::cout << _prompt_string;
    if (!std::getline(std::cin, input)) {
      break; // EOF or error
    }
    run_command(input);
  }
}

void TaskmasterCtl::run_command(const std::string &command_line) {
  _command_manager.run_command(command_line);
}

void TaskmasterCtl::set_sigint_handler() {
  struct sigaction sa = {};
  sa.sa_handler = sigint_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (sigaction(SIGINT, &sa, &_default_sigint_handler) == -1) {
    Logger::get_instance().error(std::string("sigaction: ") + strerror(errno));
    exit(EXIT_FAILURE);
  }
}

void TaskmasterCtl::reset_sigint_handler() {
  if (sigaction(SIGHUP, &_default_sigint_handler, nullptr) == -1) {
    Logger::get_instance().error(std::string("sigaction: ") + strerror(errno));
    exit(EXIT_FAILURE);
  }
}

void TaskmasterCtl::send_command(const std::vector<std::string> &args) const {
  std::string sent_command = join(args, " ");
  if (_socket.write(sent_command + '\n') == -1) {
    Logger::get_instance().error("Failed to send command: `" + sent_command +
                                 "`:" + strerror(errno));
    throw std::runtime_error(std::string("send") + strerror(errno));
  }
  Logger::get_instance().info("Command `" + sent_command + "` sent");
}

void TaskmasterCtl::send_and_receive(
    const std::vector<std::string> &args) const {
  send_command(args);
  receive_response();
}

void TaskmasterCtl::attach(const std::vector<std::string> &args) {
  send_command(args);
  set_sigint_handler();
  while (sigint_received_g == 0) {
    receive_response();
  }
  std::cout << std::endl;
  send_and_receive({CMD_DETACH_STR, args[1]});
  reset_sigint_handler();
  sigint_received_g = 0;
}

void TaskmasterCtl::quit(const std::vector<std::string> &args) {
  send_and_receive(args);
  _is_running = false;
}

void TaskmasterCtl::print_usage(const std::vector<std::string> &) const {
  std::cout << "Available commands:" << std::endl;
  for (const auto &[cmd_name, cmd] : _command_manager) {
    if (cmd.callback == nullptr) {
      continue;
    }
    std::ostringstream left_part;
    left_part << cmd.name;
    for (const auto &arg : cmd.args) {
      left_part << " " << arg;
    }
    std::cout << "  " << std::left
              << std::setw(static_cast<int>(_usage_max_len) + 2)
              << left_part.str() << cmd.doc << std::endl;
  }
}

void TaskmasterCtl::print_header() {
  std::cout << "=====================================\n";
  std::cout << "         Taskmaster Shell v1.0       \n";
  std::cout << "=====================================\n";
  std::cout << "Type 'help' to list all available commands.\n";
  std::cout << "Press Ctrl-D or Ctrl-C to quit the shell without stopping "
               "programs.\n\n";
}

void TaskmasterCtl::receive_response() const {
  char buffer[SOCKET_BUFFER_SIZE];
  ssize_t ret;

  ret = _socket.read(buffer, sizeof(buffer));
  if (ret == -1) {
    Logger::get_instance().error(std::string("Failed to read response: ") +
                                 strerror(errno));
    return;
  }
  Logger::get_instance().info("response received:\n" +
                              std::string(buffer, ret));
  std::cout << std::string(buffer, ret);
}

size_t TaskmasterCtl::get_usage_max_len() const {
  size_t max_len = 0;
  for (const auto &[cmd_name, cmd] : _command_manager) {
    size_t len = cmd.name.size();
    for (const auto &arg : cmd.args) {
      len += 1 + arg.size(); // space + arg
    }
    max_len = std::max(max_len, len);
  }
  return max_len;
}

std::unordered_map<std::string, cmd_callback_t>
TaskmasterCtl::get_commands_callback() {
  return {
      {CMD_STATUS_STR,
       [this](const std::vector<std::string> &args) {
         send_and_receive(args);
       }},
      {CMD_START_STR,
       [this](const std::vector<std::string> &args) {
         send_and_receive(args);
       }},
      {CMD_STOP_STR,
       [this](const std::vector<std::string> &args) {
         send_and_receive(args);
       }},
      {CMD_RESTART_STR,
       [this](const std::vector<std::string> &args) {
         send_and_receive(args);
       }},
      {CMD_RELOAD_STR,
       [this](const std::vector<std::string> &args) {
         send_and_receive(args);
       }},
      {CMD_QUIT_STR,
       [this](const std::vector<std::string> &args) { quit(args); }},
      {CMD_EXIT_STR,
       [this](const std::vector<std::string> &args) { quit(args); }},
      {CMD_HELP_STR,
       [this](const std::vector<std::string> &args) { print_usage(args); }},
      {CMD_ATTACH_STR,
       [this](const std::vector<std::string> &args) { attach(args); }},
      {CMD_DETACH_STR, nullptr},
  };
}

static void sigint_handler(int) { sigint_received_g = 1; }