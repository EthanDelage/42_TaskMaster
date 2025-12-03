#ifndef TASKMASTER_HPP
#define TASKMASTER_HPP

#include "PollFds.hpp"
#include "UnixSocketServer.hpp"
#include "server/ClientSession.hpp"
#include "server/Process.hpp"
#include "server/ProcessPool.hpp"

#include <common/CommandManager.hpp>
#include <sys/poll.h>
#include <unordered_map>

class Taskmaster {
public:
  explicit Taskmaster(const ConfigParser &config);
  void loop();

private:
  ConfigParser _config;
  CommandManager _command_manager;
  ProcessPool _process_pool;
  PollFds _poll_fds;
  int _wake_up_pipe[2];
  std::vector<ClientSession> _client_sessions;
  ClientSession *_current_client{};
  UnixSocketServer _server_socket;
  bool _running;

  void handle_poll_fds(PollFds::snapshot_t poll_fds_snapshot);
  void handle_client_command(const pollfd &poll_fd);
  void handle_connection(PollFds::snapshot_t poll_fds_snapshot);
  void handle_wake_up(const pollfd &poll_fd);
  void read_process_output(int fd);
  void handle_connection();
  void reload_config();
  void disconnect_client(int fd);
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
  void attach(const std::vector<std::string> &args);
  void detach(const std::vector<std::string> &args);

  // Getters
  std::vector<ClientSession>::iterator get_client_session_from_fd(int fd);
  std::unordered_map<std::string, cmd_callback_t> get_commands_callback();
};

#endif // TASKMASTER_HPP
