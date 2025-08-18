#include "server/config/Config.hpp"
#include "server/Process.hpp"
#include "server/Taskmaster.hpp"
#include <iostream>
#include <cstdlib>
#include <sys/wait.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    Config config("config.yaml");

    Taskmaster taskmaster(config);
    taskmaster.loop();
    return 0;
}
