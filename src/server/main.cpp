#include "common/Logger.hpp"
#include "server/Taskmaster.hpp"
#include "server/config/Config.hpp"
#include <exception>
#include <iostream>

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Config config("config.yaml");
  Logger::init("./server.log");

  Taskmaster taskmaster(config);
  try {
    taskmaster.loop();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
  return 0;
}
