#ifndef TASKMASTER_HPP
#define TASKMASTER_HPP

#include "server/Process.hpp"
#include "server/config/Config.hpp"

class Taskmaster {
public:
  explicit Taskmaster(Config config);
  int start();
  void loop();

private:
  std::vector<Process> _processes;
};

#endif // TASKMASTER_HPP
