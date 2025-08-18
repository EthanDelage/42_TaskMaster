#include "client/Client.hpp"

#include <common/utils.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>

Client::Client(std::string prompt_string)
    : _prompt_string(std::move(prompt_string)), _is_running(true) {
  _commands = {
      {"status", "", "Show the status of all programs from the config file",
       [this](const std::vector<std::string> &args) { status(args); }},
      {"start", "<name>", "Start the specified program",
       [this](const std::vector<std::string> &args) { start(args); }},
      {"stop", "<name>", "Stop the specified program",
       [this](const std::vector<std::string> &args) { stop(args); }},
      {"restart", "<name>", "Restart the specified program",
       [this](const std::vector<std::string> &args) { restart(args); }},
      {"reload", "", "Reload the configuration file without stopping the shell",
       [this](const std::vector<std::string> &args) { reload(args); }},
      {"quit", "", "Stop all programs and exit the shell",
       [this](const std::vector<std::string> &args) { quit(args); }},
      {"exit", "", "Same as 'quit'",
       [this](const std::vector<std::string> &args) { quit(args); }},
      {"help", "", "Show this help message",
       [this](const std::vector<std::string> &args) { print_usage(args); }},
  };
  _usage_max_len = get_usage_max_len();
}

void Client::loop() {
  std::string input;

  print_header();
  while (_is_running) {
    std::cout << _prompt_string;
    if (!std::getline(std::cin, input)) {
      break; // EOF or error
    }

    std::vector<std::string> args = split(input, ' ');
    if (args.empty()) {
      continue;
    }
    run_command(args);
  }
}

void Client::run_command(const std::vector<std::string> &args) {
  const std::string &cmd = args[0];
  for (auto &_command : _commands) {
    if (_command.name == cmd) {
      _command.func(args);
      return;
    }
  }
  std::cerr << "Unknown command: " << cmd << "\n";
}

void Client::status(const std::vector<std::string> &) {
  std::cout << "Status" << std::endl;
}

void Client::start(const std::vector<std::string> &args) {
  if (args.size() != 2) {
    std::cerr << "Invalid number of arguments" << std::endl
              << "Usage: start <program_name>" << std::endl;
    return;
  }
  std::cout << "Starting process: " << args[1] << std::endl;
}

void Client::stop(const std::vector<std::string> &args) {
  if (args.size() != 2) {
    std::cerr << "Invalid number of arguments" << std::endl
              << "Usage: stop <program_name>" << std::endl;
    return;
  }
  std::cout << "Stopping process: " << args[1] << std::endl;
}

void Client::restart(const std::vector<std::string> &args) {
  if (args.size() != 2) {
    std::cerr << "Invalid number of arguments" << std::endl
              << "Usage: restart <program_name>" << std::endl;
    return;
  }
  std::cout << "Restarting process: " << args[1] << std::endl;
}

void Client::reload(const std::vector<std::string> &) {
  std::cout << "Reloading config" << std::endl;
}

void Client::quit(const std::vector<std::string> &) { _is_running = false; }

void Client::print_header() {
  std::cout << "=====================================\n";
  std::cout << "         Taskmaster Shell v1.0       \n";
  std::cout << "=====================================\n";
  std::cout << "Type 'help' to list all available commands.\n";
  std::cout << "Press Ctrl-D or Ctrl-C to quit the shell without stopping "
               "programs.\n\n";
}

void Client::print_usage(const std::vector<std::string> &) const {
  std::cout << "Available commands:\n";
  for (const auto &cmd : _commands) {
    std::ostringstream left_part;
    left_part << cmd.name;
    if (!cmd.arg.empty()) {
      left_part << " " << cmd.arg;
    }

    std::cout << "  " << std::left
              << std::setw(static_cast<int>(_usage_max_len) + 8)
              << left_part.str() << cmd.doc << std::endl;
  }
}

size_t Client::get_usage_max_len() const {
  size_t max_len = 0;
  for (const auto &cmd : _commands) {
    size_t len = cmd.name.size() + (cmd.arg.empty() ? 0 : 1 + cmd.arg.size());
    max_len = std::max(max_len, len);
  }
  return max_len;
}
