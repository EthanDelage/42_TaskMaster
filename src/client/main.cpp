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
    Logger::get_instance().error(e.what());
  }
  return 0;
}
