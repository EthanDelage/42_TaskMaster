#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "UnixSocketClient.hpp"

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

class Client {
public:
  explicit Client(std::string prompt_string);
  void loop();
  void run_command(const std::string &command_line);

private:
  std::unordered_map<std::string, client_command_t> _commands_map;
  size_t _usage_max_len;
  std::string _prompt_string;
  bool _is_running;
  UnixSocketClient _socket;

  void add_command(const client_command_t &command);
  void send_command_and_print(const std::vector<std::string> &args) const;
  void quit(const std::vector<std::string> &);
  void print_usage(const std::vector<std::string> &) const;
  static void print_header();
  void print_feedback() const;
  size_t get_usage_max_len() const;
  static bool is_valid_args(const client_command_t &command,
                            const std::vector<std::string> &args);
};

#endif // CLIENT_HPP
