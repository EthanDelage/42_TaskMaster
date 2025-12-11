#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

void sigusr1_hanler(int sig) {
  printf("Received signal %d\n", sig);
  fflush(stdout);
}

void sigusr2_hanler(int sig) {
  printf("Received signal %d\n", sig);
  fflush(stdout);
  exit(0);
}

int main() {
  signal(SIGUSR1, sigusr1_hanler);
  signal(SIGUSR2, sigusr2_hanler);
  while (1) {}
}
