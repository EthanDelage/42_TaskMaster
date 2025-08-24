#include "client/Client.hpp"

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Client client = Client("$> ");

  client.loop();
  return 0;
}
