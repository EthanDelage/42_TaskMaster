#include "client/Client.hpp"

int main() {
    Client client = Client("$> ");

    client.loop();
    return 0;
}
