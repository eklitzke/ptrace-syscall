#include <errno.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>

// This sets up the following two instructions:
//   call *%rax
//   int3
//
// This should let us set the target address in %rax, continue, wait for a TRAP
// to occur, and then reset the state of things
static const long call_rax = 0xccd0ff;

int main(int argc, char **argv) {
  if (argc != 3) {
    printf("usage: %s <pid> <addr>\n", argv[0]);
    return 1;
  }

  long pid = strtol(argv[1], NULL, 10);
  if (pid <= 1 || pid == LONG_MAX) {
    printf("cowardly refusing to ptrace pid %ld\n", pid);
    return 1;
  }

  long addr = strtol(argv[2], NULL, 16);
  printf("using pid %ld addr %p\n", pid, (void*)addr);
  if (addr == LONG_MIN || addr == LONG_MAX) {
    printf("failed to decode addr %s in base 16", argv[2]);
    return 1;
  }

  // attach the target
  if (ptrace(PTRACE_ATTACH, pid, 0, 0)) {
    perror("PTRACE_ATTACH");
    return 1;
  }

  // wait for stop and check stop status
  int status;
  if (wait(&status) == -1) {
    perror("wait()");
    return 1;
  }
  if (!WIFSTOPPED(status)) {
    printf("for some reason the target did not stop\n");
    return 1;
  }

  // get the original registers
  struct user_regs_struct old_regs, new_regs;
  if (ptrace(PTRACE_GETREGS, pid, 0, &old_regs)) {
    perror("ptrace(PTRACE_GETREGS...)");
    return 1;
  }
  printf("stoppped target at %%rip = %p\n", (void*)old_regs.rip);

  long orig_word;
  errno = 0;
  orig_word = ptrace(PTRACE_PEEKTEXT, pid, old_regs.rip, 0);
  if (errno) {
    perror("ptrace(PTRACE_PEEKTEXT...)");
    return 1;
  }

  if (ptrace(PTRACE_POKETEXT, pid, old_regs.rip, call_rax)) {
    perror("ptrace(PTRACE_POKETEXT...)");
    return 1;
  }
  memmove(&new_regs, &old_regs, sizeof(old_regs));
  new_regs.rax = addr;
  if (ptrace(PTRACE_SETREGS, pid, 0, &new_regs)) {
    perror("ptrace(PTRACE_SETREGS...)");
    return 1;
  }
  if (ptrace(PTRACE_CONT, pid, 0, 0)) {
    perror("ptrace(PTRACE_CONT...)");
    return 1;
  }
  if (wait(&status) == -1) {
    perror("wait()");
    return 1;
  }
  if (WIFEXITED(status)) {
    printf("target failed with %s\n", strsignal(WEXITSTATUS(status)));
    return 1;
  }
  if (!WIFSTOPPED(status)) {
    printf("for some reason the target did not stop\n");
    return 1;
  }
  printf("success!\n");

  // reset the old program state
  if (ptrace(PTRACE_POKETEXT, pid, old_regs.rip, orig_word)) {
    perror("ptrace(PTRACE_POKETEXT...)");
    return 1;
  }
  if (ptrace(PTRACE_SETREGS, pid, 0, &old_regs)) {
    perror("ptrace(PTRACE_SETREGS...)");
    return 1;
  }

  return 0;
}
