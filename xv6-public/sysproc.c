#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "klog.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// Retrieve kernel log snapshot into user buffer (backward-compatible)
int
sys_getklog(void)
{
  int buf_addr;
  int max_entries;
  struct klog_entry *kbuf;
  int count, i;
  int max_fit;
  struct proc *curproc = myproc();
  
  if(argint(0, &buf_addr) < 0 || argint(1, &max_entries) < 0)
    return -1;
  
  if(max_entries <= 0 || max_entries > 1024)
    return -1;
  
  if(buf_addr < 0 || (uint)buf_addr >= curproc->sz)
    return -1;
  
  kbuf = (struct klog_entry*)kalloc();
  if(!kbuf)
    return -1;
  
  max_fit = PGSIZE / sizeof(struct klog_entry);
  if(max_entries > max_fit)
    max_entries = max_fit;
  
  if((uint)buf_addr + max_entries * sizeof(struct klog_entry) > curproc->sz){
    kfree((char*)kbuf);
    return -1;
  }
  
  count = klog_snapshot(kbuf, max_entries);
  
  for(i = 0; i < count; i++){
    if(copyout(curproc->pgdir, (uint)buf_addr + i * sizeof(struct klog_entry),
               &kbuf[i], sizeof(struct klog_entry)) < 0){
      kfree((char*)kbuf);
      return -1;
    }
  }
  
  kfree((char*)kbuf);
  return count;
}

// Administrative and operational control system call for kernel logging subsystem
int
sys_klogctl(void)
{
  int cmd;
  int arg_addr;
  int len;
  struct proc *curproc = myproc();
  
  if(argint(0, &cmd) < 0)
    return -1;
  
  switch(cmd){
  case KLOG_CMD_READ: {
    struct klog_entry *kbuf;
    int count, i, max_fit;
    
    if(argint(1, &arg_addr) < 0 || argint(2, &len) < 0)
      return -1;
    if(len <= 0 || len > 1024 || arg_addr < 0 || (uint)arg_addr >= curproc->sz)
      return -1;
    
    kbuf = (struct klog_entry*)kalloc();
    if(!kbuf)
      return -1;
    
    max_fit = PGSIZE / sizeof(struct klog_entry);
    if(len > max_fit)
      len = max_fit;
    
    if((uint)arg_addr + len * sizeof(struct klog_entry) > curproc->sz){
      kfree((char*)kbuf);
      return -1;
    }
    
    count = klog_snapshot(kbuf, len);
    for(i = 0; i < count; i++){
      if(copyout(curproc->pgdir, (uint)arg_addr + i * sizeof(struct klog_entry),
                 &kbuf[i], sizeof(struct klog_entry)) < 0){
        kfree((char*)kbuf);
        return -1;
      }
    }
    kfree((char*)kbuf);
    return count;
  }
  
  case KLOG_CMD_CLEAR:
    klog_clear();
    return 0;
    
  case KLOG_CMD_STATS: {
    struct klog_stats st;
    if(argint(1, &arg_addr) < 0 || argint(2, &len) < 0)
      return -1;
    if(len < (int)sizeof(struct klog_stats) || arg_addr < 0 || (uint)arg_addr + sizeof(struct klog_stats) > curproc->sz)
      return -1;
    
    klog_get_stats(&st);
    if(copyout(curproc->pgdir, (uint)arg_addr, &st, sizeof(struct klog_stats)) < 0)
      return -1;
    return 0;
  }
  
  default:
    return -1;
  }
}

