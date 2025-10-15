#ifndef TASKMASTER_HPP
#define TASKMASTER_HPP

#include "UnixSocketServer.hpp"
#include "server/Process.hpp"
#include "server/config/Config.hpp"

#include <common/CommandManager.hpp>
#include <sys/poll.h>
#include <unordered_map>

enum class FdType {
  Server,
  Client,
  Process,
};

typedef struct poll_fd_metadata_s poll_fd_metadata_t;
struct poll_fd_metadata_s {
  FdType type;
};

class Taskmaster {
public:
  explicit Taskmaster(const Config &config);
  void loop();

private:
  Config _config;
  CommandManager _command_manager;
  std::unordered_map<std::string, std::vector<Process>> _process_pool;
  std::vector<pollfd> _poll_fds;
  std::vector<poll_fd_metadata_t> _poll_fds_metadata;
  UnixSocketServer _server_socket;

  void init_process_pool(std::vector<ProgramConfig>& programs_configs);
  void handle_poll_fds();
  void handle_client_command();
  void read_process_output(int fd);
  void handle_connection();
  void disconnect_client(int fd);
  void add_poll_fd(pollfd fd, poll_fd_metadata_t metadata);
  void remove_poll_fd(int fd);
  static void set_sighup_handler();

  // Callback
  void status(const std::vector<std::string> &args);
  void start(const std::vector<std::string> &args);
  void stop(const std::vector<std::string> &args);
  void restart(const std::vector<std::string> &args);
  void reload(const std::vector<std::string> &args);
  void quit(const std::vector<std::string> &args);
  void help(const std::vector<std::string> &args);

  // Getters
  std::unordered_map<std::string, cmd_callback_t> get_commands_callback();
};

#endif // TASKMASTER_HPP
