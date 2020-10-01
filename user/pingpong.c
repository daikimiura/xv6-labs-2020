#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[]) {
  // 0: read, 1: write
  int pipe_child2parent[2], pipe_parent2child[2];
  int pid;

  if (pipe(pipe_child2parent) < 0) {
    fprintf(2, "pipe creation failed\n");
    exit(1);
  }

  if (pipe(pipe_parent2child) < 0) {
    fprintf(2, "pipe creation failed\n");
    exit(1);
  }

  pid = fork();

  if (pid < 0) {
    fprintf(2, "fork failed\n");
    exit(1);
  }

  char *s = "m";
  if (pid == 0) {
    char child_buf[128];
    read(pipe_parent2child[0], child_buf, sizeof(child_buf));
    printf("%d: received ping\n", getpid());

    write(pipe_child2parent[1], child_buf, strlen(s));
    exit(0);
  } else {
    char parent_buf[128];
    write(pipe_parent2child[1], s, strlen(s));

    read(pipe_child2parent[0], parent_buf, sizeof(parent_buf));
    printf("%d: received pong\n", getpid());
    exit(0);
  }
}
