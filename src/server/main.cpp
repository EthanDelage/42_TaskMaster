#include <server/config/Config.hpp>
#include <iostream>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    Config configs;
    configs.parse("config.yaml");

    std::cout << configs;
    return 0;
}
