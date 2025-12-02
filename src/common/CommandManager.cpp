#include "common/CommandManager.hpp"

#include <common/utils.hpp>
#include <iostream>

static cmd_callback_t get_command_callback(
    const std::string &command_name,
    const std::unordered_map<std::string, cmd_callback_t> &commands_callback);

CommandManager::CommandManager(
    const std::unordered_map<std::string, cmd_callback_t> &commands_callback) {
  add_command({
      CMD_STATUS_STR,
      {},
      "Show the status of all programs from the config file",
      get_command_callback(CMD_STATUS_STR, commands_callback),
  });
  add_command({
      CMD_START_STR,
      {"<program_name>"},
      "Start the specified program",
      get_command_callback(CMD_START_STR, commands_callback),
  });
  add_command({
      CMD_STOP_STR,
      {"<program_name>"},
      "Stop the specified program",
      get_command_callback(CMD_STOP_STR, commands_callback),
  });
  add_command({
      CMD_RESTART_STR,
      {"<program_name>"},
      "Restart the specified program",
      get_command_callback(CMD_RESTART_STR, commands_callback),
  });
  add_command({
      CMD_RELOAD_STR,
      {},
      "Reload the configuration file without stopping the shell",
      get_command_callback(CMD_RELOAD_STR, commands_callback),
  });
  add_command({
      CMD_QUIT_STR,
      {},
      "Same as 'exit'",
      get_command_callback(CMD_QUIT_STR, commands_callback),
  });
  add_command({
      CMD_EXIT_STR,
      {},
      "Stop all programs and exit the shell",
      get_command_callback(CMD_EXIT_STR, commands_callback),
  });
  add_command({
      CMD_HELP_STR,
      {},
      "Show this help message",
      get_command_callback(CMD_HELP_STR, commands_callback),
  });
}

void CommandManager::run_command(const std::string &command_line) {
  const std::vector<std::string> args = split(command_line, ' ');
  if (args.empty()) {
    return;
  }
  const std::string &cmd_name = args[0];
  const auto cmd = _commands_map.find(cmd_name);

  if (cmd != _commands_map.end() && cmd->second.callback != nullptr) {
    if (is_valid_args(cmd->second, args)) {
      cmd->second.callback(args);
    }
  } else {
    std::cerr << "Unknown command: `" << cmd_name << '`' << std::endl;
  }
}

void CommandManager::add_command(const command_t &command) {
  _commands_map.emplace(command.name, command);
}

bool CommandManager::is_valid_args(const command_t &command,
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

CommandManager::iterator CommandManager::begin() {
  return _commands_map.begin();
}

CommandManager::iterator CommandManager::end() { return _commands_map.end(); }

CommandManager::const_iterator CommandManager::begin() const {
  return _commands_map.begin();
}

CommandManager::const_iterator CommandManager::end() const {
  return _commands_map.end();
}

CommandManager::const_iterator CommandManager::cbegin() const {
  return _commands_map.cbegin();
}

CommandManager::const_iterator CommandManager::cend() const {
  return _commands_map.cend();
}

static cmd_callback_t get_command_callback(
    const std::string &command_name,
    const std::unordered_map<std::string, cmd_callback_t> &commands_callback) {
  const auto it = commands_callback.find(command_name);
  if (it == commands_callback.end()) {
    throw std::runtime_error("CommandManager(): Command `" + command_name +
                             "` has no defined callback");
  }
  return it->second;
}
