#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc < 2){
    fprintf(2, "Usage: sleep [#ticks]\n");
    exit(1);
  }

  int seconds = atoi(argv[1]);
  sleep(seconds);

  exit(0);
}
