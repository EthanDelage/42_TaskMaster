#include "client/Client.hpp"

#include <common/utils.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>

Client::Client(std::string prompt_string)
    : _prompt_string(std::move(prompt_string)), _is_running(true),
      _socket(SOCKET_PATH_NAME) {
  add_command({"status",
               {},
               "Show the status of all programs from the config file",
               [this](const std::vector<std::string> &args) {
                 send_command_and_print(args);
               }});
  add_command({"start",
               {"<program_name>"},
               "Start the specified program",
               [this](const std::vector<std::string> &args) {
                 send_command_and_print(args);
               }});
  add_command({"stop",
               {"<program_name>"},
               "Stop the specified program",
               [this](const std::vector<std::string> &args) {
                 send_command_and_print(args);
               }});
  add_command({"restart",
               {"<program_name>"},
               "Restart the specified program",
               [this](const std::vector<std::string> &args) {
                 send_command_and_print(args);
               }});
  add_command({"reload",
               {},
               "Reload the configuration file without stopping the shell",
               [this](const std::vector<std::string> &args) {
                 send_command_and_print(args);
               }});
  add_command({"quit",
               {},
               "Same as 'exit'",
               [this](const std::vector<std::string> &args) { quit(args); }});
  add_command({"exit",
               {},
               "Stop all programs and exit the shell",
               [this](const std::vector<std::string> &args) { quit(args); }});
  add_command(
      {"help",
       {},
       "Show this help message",
       [this](const std::vector<std::string> &args) { print_usage(args); }});
  _usage_max_len = get_usage_max_len();
  _socket.connect();
}

void Client::loop() {
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

void Client::run_command(const std::string &command_line) {
  const std::vector<std::string> args = split(command_line, ' ');
  if (args.empty()) {
    return;
  }
  const std::string &cmd_name = args[0];
  const auto cmd = _commands_map.find(cmd_name);

  if (cmd != _commands_map.end()) {
    if (is_valid_args(cmd->second, args)) {
      cmd->second.func(args);
    }
  } else {
    std::cerr << "Unknown command: `" << cmd_name << '`' << std::endl;
  }
}

void Client::send_command_and_print(
    const std::vector<std::string> &args) const {
  if (_socket.send(join(args, " ")) == -1) {
    throw std::runtime_error(std::string("send") + strerror(errno));
  }
  std::cout << "message sent" << std::endl;
  receive_response();
}

void Client::quit(const std::vector<std::string> &args) {
  send_command_and_print(args);
  _is_running = false;
}

void Client::print_usage(const std::vector<std::string> &) const {
  std::cout << "Available commands:" << std::endl;
  for (const auto &[cmd_name, cmd] : _commands_map) {
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

void Client::print_header() {
  std::cout << "=====================================\n";
  std::cout << "         Taskmaster Shell v1.0       \n";
  std::cout << "=====================================\n";
  std::cout << "Type 'help' to list all available commands.\n";
  std::cout << "Press Ctrl-D or Ctrl-C to quit the shell without stopping "
               "programs.\n\n";
}

void Client::receive_response() const {
  try {
    const std::string feedback = _socket.receive();
    std::cout << feedback << std::endl;
  } catch (const std::runtime_error &e) {
    std::cerr << e.what() << std::endl;
  }
}

void Client::add_command(const client_command_t &command) {
  _commands_map.emplace(command.name, command);
}

size_t Client::get_usage_max_len() const {
  size_t max_len = 0;
  for (const auto &[cmd_name, cmd] : _commands_map) {
    size_t len = cmd.name.size();
    for (const auto &arg : cmd.args) {
      len += 1 + arg.size(); // space + arg
    }
    max_len = std::max(max_len, len);
  }
  return max_len;
}

bool Client::is_valid_args(const client_command_t &command,
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
