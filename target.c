#include <stdio.h>
#include <unistd.h>
#include <sys/select.h>

void hello() { fprintf(stderr, "hello\n"); }

int main(int argc, char **argv) {
  printf("%d %p\n", getpid(), hello);
  for (;;) {
    printf("entering infinite select...\n");
    select(0, NULL, NULL, NULL, NULL);
  }
}
