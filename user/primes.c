#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
pipe_prime(int left_read_fd) {
  int num, pid, i;
  int right_pipe[2];

  top:
  if (read(left_read_fd, &num, sizeof(int)) != 0) { ;
    printf("prime %d\n", num);
  } else {
    exit(0);
  }

  pipe(right_pipe);
  pid = fork();
  if (pid < 0) {
    fprintf(2, "failed to fork");
    exit(1);
  } else if (pid == 0) {
    close(left_read_fd);
    close(right_pipe[1]);
    left_read_fd = right_pipe[0];
    goto top;
  } else {
    close(right_pipe[0]);
    while (read(left_read_fd, &i, sizeof(i)) == sizeof(i)) {
      if (i % num != 0) {
        write(right_pipe[1], &i, sizeof(i));
      }
    }
    close(right_pipe[1]);
    int status;
    wait(&status);
    exit(0);
  }
}

int
main(int argc, char *argv[]) {
  int p[2];
  pipe(p);

  int pid = fork();
  if (pid < 0) {
    fprintf(2, "failed to fork");
    exit(1);
  } else if (pid == 0) {
    close(p[1]);
    pipe_prime(p[0]);
    exit(0);
  } else {
    close(p[0]);
    for (int i = 2; i <= 35; i++) {
      write(p[1], &i, sizeof(i));
    }
    close(p[1]);
    int status;
    wait(&status);
    exit(0);
  }
}

