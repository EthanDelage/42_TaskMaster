#include "server/Taskmaster.hpp"
#include "server/config/Config.hpp"
#include <exception>
#include <iostream>

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  if (daemon(0, 0) == -1) {
    perror("daemon");
  }
  Config config("config.yaml");

  Taskmaster taskmaster(config);
  try {
    taskmaster.loop();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
  return 0;
}
