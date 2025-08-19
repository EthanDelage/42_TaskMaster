#include "server/Process.hpp"
#include "server/Taskmaster.hpp"
#include "server/config/Config.hpp"
#include <cstdlib>
#include <iostream>
#include <sys/wait.h>

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Config config("config.yaml");

  Taskmaster taskmaster(config);
  taskmaster.loop();
  return 0;
}
