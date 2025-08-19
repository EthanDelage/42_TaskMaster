#include "server/Taskmaster.hpp"

#include <iostream>

Taskmaster::Taskmaster(Config config) {
  std::vector<ProgramConfig> program_configs = config.parse();

  for (auto &config : program_configs) {
    _processes.push_back(Process(config));
  }
}

void Taskmaster::loop() {
  start();
  while (true) {
    return;
  }
}

int Taskmaster::start() {
  for (Process &process : _processes) {
    if (process.get_program_config().get_autostart()) {
      std::cout << "[Taskmaster] Starting "
                << process.get_program_config().get_name() << "..."
                << std::endl;
      process.start();
    }
  }
  return 0;
}
