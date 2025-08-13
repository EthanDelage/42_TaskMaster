#include <server/config/Config.hpp>
#include <iostream>
#include "server/Process.hpp"
#include <cstdlib>
#include <sys/wait.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    Config configs("config.yaml");
    configs.parse();

    std::cout << configs;

    Process ls = Process("ls");
    ls.start();
    wait(nullptr);
    return 0;
}
