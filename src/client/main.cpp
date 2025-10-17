#include "client/TaskmasterCtl.hpp"

#include <common/Logger.hpp>
#include <iostream>
#include <ostream>

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Logger::init("./client.log");
  try {
    TaskmasterCtl ctl = TaskmasterCtl("$> ");
    ctl.loop();
  } catch (const std::runtime_error &e) {
    std::cerr << e.what() << std::endl;
  }
  return 0;
}
