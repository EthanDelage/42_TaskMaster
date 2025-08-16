#ifndef PROCESS_HPP
#define PROCESS_HPP

#include <string>

class Process {
public:
  explicit Process(std::string cmd);

  int start();
  int stop(int sig);
  int restart(int sig);

  pid_t getpid() const;

private:
  static std::string get_cmd_path(const std::string &cmd);

  // TODO: change cmd by program_config (when it is implemented)
  std::string _cmd;
  std::string _cmd_path;
  pid_t _pid;
};

#endif // PROCESS_HPP
