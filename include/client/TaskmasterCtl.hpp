#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "UnixSocketClient.hpp"

#include <common/CommandManager.hpp>
#include <functional>
#include <string>
#include <vector>

typedef struct client_command_s client_command_t;
struct client_command_s {
  std::string name;
  std::vector<std::string> args;
  std::string doc;
  std::function<void(const std::vector<std::string> &)> func;
};

class TaskmasterCtl {
public:
  explicit TaskmasterCtl(std::string prompt_string);
  void loop();
  void run_command(const std::string &command_line);

private:
  CommandManager _command_manager;
  size_t _usage_max_len;
  std::string _prompt_string;
  bool _is_running;
  UnixSocketClient _socket;

  void send_command_and_print(const std::vector<std::string> &args) const;
  void quit(const std::vector<std::string> &);
  void print_usage(const std::vector<std::string> &) const;
  static void print_header();
  void receive_response() const;
  size_t get_usage_max_len() const;
  static bool is_valid_args(const client_command_t &command,
                            const std::vector<std::string> &args);
  std::unordered_map<std::string, cmd_callback_t> get_commands_callback();
};

#endif // CLIENT_HPP
