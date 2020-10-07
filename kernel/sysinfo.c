//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

uint64
sys_sysinfo(void)
{
  uint64 addr; // user pointer to struct stat

  if (argaddr(0, &addr) < 0)
    return -1;

  struct sysinfo info;
  info.freemem = fetch_free_memory_bytes();
  info.nproc = fetch_proc_num();

  struct proc *p = myproc();
  if (copyout(p->pagetable, addr, (char *)&info, sizeof(info)) < 0)
    return -1;
  return 0;
}