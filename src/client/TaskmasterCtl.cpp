#include "client/TaskmasterCtl.hpp"

#include <common/utils.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>

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

void TaskmasterCtl::send_command_and_print(
    const std::vector<std::string> &args) const {
  if (_socket.write(join(args, " ") + '\n') == -1) {
    throw std::runtime_error(std::string("send") + strerror(errno));
  }
  std::cout << "message sent" << std::endl;
  receive_response();
}

void TaskmasterCtl::quit(const std::vector<std::string> &args) {
  send_command_and_print(args);
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
    return;
  }
  std::cout << std::string(buffer, ret) << std::endl;
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

bool TaskmasterCtl::is_valid_args(const client_command_t &command,
                                  const std::vector<std::string> &args) {
  if (command.args.size() + 1 != args.size()) {
    std::cerr << "Invalid number of arguments" << std::endl
              << "Usage: " << command.name;
    for (const auto &arg : command.args) {
      std::cerr << ' ' << arg;
    }
    std::cerr << std::endl;
    return false;
  }
  return true;
}

std::unordered_map<std::string, cmd_callback_t>
TaskmasterCtl::get_commands_callback() {
  return {
      {CMD_STATUS_STR,
       [this](const std::vector<std::string> &args) {
         send_command_and_print(args);
       }},
      {CMD_START_STR,
       [this](const std::vector<std::string> &args) {
         send_command_and_print(args);
       }},
      {CMD_STOP_STR,
       [this](const std::vector<std::string> &args) {
         send_command_and_print(args);
       }},
      {CMD_RESTART_STR,
       [this](const std::vector<std::string> &args) {
         send_command_and_print(args);
       }},
      {CMD_RELOAD_STR,
       [this](const std::vector<std::string> &args) {
         send_command_and_print(args);
       }},
      {CMD_QUIT_STR,
       [this](const std::vector<std::string> &args) { quit(args); }},
      {CMD_EXIT_STR,
       [this](const std::vector<std::string> &args) { quit(args); }},
      {CMD_HELP_STR,
       [this](const std::vector<std::string> &args) { print_usage(args); }},
  };
}
