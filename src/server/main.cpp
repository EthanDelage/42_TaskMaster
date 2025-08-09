#include <server/config/Config.hpp>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    Config configs("config.yaml");

    configs.print();
    return 0;
}
