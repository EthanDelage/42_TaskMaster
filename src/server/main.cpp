#include "server/Taskmaster.hpp"
#include "server/config/Config.hpp"
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <sys/wait.h>

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Config config("config.yaml");

  Taskmaster taskmaster(config);
  try {
    taskmaster.loop();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
  return 0;
}
