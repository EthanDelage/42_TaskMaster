#include "client/Client.hpp"

#include <iostream>
#include <ostream>

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  try {
    Client client = Client("$> ");
    client.loop();
  } catch (const std::runtime_error &e) {
    std::cerr << e.what() << std::endl;
  }
  return 0;
}
