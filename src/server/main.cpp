#include "common/Logger.hpp"
#include "server/ConfigParser.hpp"
#include "server/Taskmaster.hpp"
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <sys/wait.h>

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " <config_file>" << std::endl;
    return 0;
  }
  Logger::init("./server.log");
  try {
    ConfigParser config(argv[1]);
    Taskmaster taskmaster(config);
    taskmaster.loop();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
  return 0;
}
