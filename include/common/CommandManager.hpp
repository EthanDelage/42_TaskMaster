#ifndef COMMAND_HPP
#define COMMAND_HPP

#include <functional>
#include <string>

#define CMD_STATUS_STR "status"
#define CMD_START_STR "start"
#define CMD_STOP_STR "stop"
#define CMD_RESTART_STR "restart"
#define CMD_RELOAD_STR "reload"
#define CMD_QUIT_STR "quit"
#define CMD_EXIT_STR "exit"
#define CMD_HELP_STR "help"
#define CMD_UNKNOWN_STR "unknown"

typedef std::function<void(const std::vector<std::string> &)> cmd_callback_t;

typedef struct command_s command_t;
struct command_s {
  std::string name;
  std::vector<std::string> args;
  std::string doc;
  cmd_callback_t callback;
};

class CommandManager {

public:
  using iterator = std::unordered_map<std::string, command_s>::iterator;
  using const_iterator =
      std::unordered_map<std::string, command_s>::const_iterator;

  explicit CommandManager(
      const std::unordered_map<std::string, cmd_callback_t> &commands_callback);

  void run_command(const std::string &command_line);

  iterator begin();
  iterator end();
  const_iterator begin() const;
  const_iterator end() const;
  const_iterator cbegin() const;
  const_iterator cend() const;

private:
  std::unordered_map<std::string, command_s> _commands_map;

  void add_command(const command_t &command);
  static bool is_valid_args(const command_t &command,
                            const std::vector<std::string> &args);
};

#endif // COMMAND_HPP
