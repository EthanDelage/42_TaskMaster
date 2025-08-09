#include "server/Process.hpp"

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    Process ls = Process("ls");
    ls.start();
    wait(nullptr);
    return 0;
}
