#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if (argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if (argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if (argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (myproc()->killed)
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  backtrace();
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if (argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_sigalarm(void)
{
  int ticks;
  void (*handler)();
  uint64 f_addr;

  argint(0, &ticks);
  argaddr(1, &f_addr);

  handler = (void (*)())f_addr;

  struct proc *p = myproc();
  p->is_alarm_available = 1;
  p->alarm_interval_ticks = ticks;
  p->alarm_handler = handler;

  return 0;
}

uint64
sys_sigreturn(void)
{
  struct trapframe *tf = myproc()->trapframe;
  struct proc *p = myproc();
  tf->t6 = p->t6;
  tf->t5 = p->t5;
  tf->t4 = p->t4;
  tf->t3 = p->t3;
  tf->s11 = p->s11;
  tf->s10 = p->s10;
  tf->s9 = p->s9;
  tf->s8 = p->s8;
  tf->s7 = p->s7;
  tf->s6 = p->s6;
  tf->s5 = p->s5;
  tf->s4 = p->s4;
  tf->s3 = p->s3;
  tf->s2 = p->s2;
  tf->a7 = p->a7;
  tf->a6 = p->a6;
  tf->a5 = p->a5;
  tf->a4 = p->a4;
  tf->a3 = p->a3;
  tf->a2 = p->a2;
  tf->a1 = p->a1;
  tf->a0 = p->a0;
  tf->s1 = p->s1;
  tf->s0 = p->s0;
  tf->t2 = p->t2;
  tf->t1 = p->t1;
  tf->t0 = p->t0;
  tf->tp = p->tp;
  tf->gp = p->gp;
  tf->sp = p->sp;
  tf->ra = p->ra;
  tf->epc = p->epc;

  p->is_alarm_handler_running = 0;
  return 0;
}