#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    fprintf(2, "Usage: xargs [command]");
    exit(1);
  };

  char buf[256];
  int args_len;

  while (gets(buf, sizeof(buf)))
  {
    if (strlen(buf) <= 0)
      break;
    args_len = argc - 1;
    char *args[MAXARG];
    for (int i = 1; i < argc; i++)
    {
      args[i - 1] = argv[i];
    }

    buf[strlen(buf) - 1] = '\0';
    char *p = buf;

    while (*p)
    {
      args[args_len++] = p;
      while (*p != ' ')
      {
        p++;
      }

      p++;
    }


    // for (int i = 0; i < args_len; i++)
    // {
    //   printf("args: %s\n", args[i]);
    // }

    if (fork() == 0)
    {
      // printf("exec: %s\n", args[0]);
      // printf("=============\n");
      exec(args[0], args);
      exit(1);
    }
    else
    {
      wait(0);
    }
  };
  exit(0);
}