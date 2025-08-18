#ifndef TASKMASTER_HPP
#define TASKMASTER_HPP

#include "server/config/Config.hpp"
#include "server/Process.hpp"

class Taskmaster {
public:
  explicit Taskmaster(Config config);
  int start();
  void loop();

private:
  std::vector<Process> _processes;
};

#endif // PROCESS_HPP
