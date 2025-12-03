#ifndef TASKMASTER_HPP
#define TASKMASTER_HPP

#include "UnixSocketServer.hpp"
#include "server/ClientSession.hpp"
#include "server/Process.hpp"
#include "server/ProcessPool.hpp"

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
  explicit Taskmaster(const ConfigParser &config);
  void loop();

private:
  ConfigParser _config;
  CommandManager _command_manager;
  ProcessPool _process_pool;
  std::vector<pollfd> _poll_fds;
  std::vector<poll_fd_metadata_t> _poll_fds_metadata;
  std::vector<ClientSession> _client_sessions;
  ClientSession *_current_client{};
  UnixSocketServer _server_socket;
  bool _running;

  void init_process_pool(
      std::unordered_map<std::string, process_config_t> &process_configs);
  void register_pool(ProcessPool process_pool);
  void unregister_pool(ProcessPool process_pool);
  void insert_process(process_config_t &process_config);
  void remove_process(std::string const &process_name);
  void remove_stale(ProcessPool &new_pool);
  void handle_poll_fds();
  void handle_client_command(const pollfd &poll_fd);
  void read_process_output(int fd);
  void reload_config();
  void handle_connection();
  void disconnect_client(int fd);
  void add_poll_fd(pollfd fd, poll_fd_metadata_t metadata);
  void remove_poll_fd(int fd);
  void request_command(const std::vector<std::string> &args,
                       Process::Command command);
  void remove_client_session(int fd);
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
  std::vector<ClientSession>::iterator get_client_session_from_fd(int fd);
  std::unordered_map<std::string, cmd_callback_t> get_commands_callback();
};

#endif // TASKMASTER_HPP
