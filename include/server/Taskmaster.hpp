#ifndef TASKMASTER_HPP
#define TASKMASTER_HPP

#include "UnixSocketServer.hpp"
#include "server/Process.hpp"
#include "server/config/Config.hpp"

#include <common/CommandManager.hpp>
#include <sys/poll.h>
#include <unordered_map>

#define SERVER_SOCKFD_INDEX 0
#define BUFFER_SIZE 1024

class Taskmaster {
public:
  explicit Taskmaster(Config config);
  int autostart_processes();
  void loop();

private:
  CommandManager _command_manager;
  std::vector<Process> _processes;
  UnixSocketServer _socket;
  std::vector<pollfd> _poll_fds;
  std::unordered_map<pid_t, Process &> _running_processes;

  void start_process(Process &process);
  void reap_processes();
  void process_termination_handler(pid_t pid, int exitcode);
  void process_poll_fds();
  void handle_connection();
  static std::string read_command(int fd);
  std::unordered_map<std::string, cmd_callback_t> get_commands_callback();
  void callback(const std::vector<std::string> &args);
};

#endif // TASKMASTER_HPP
