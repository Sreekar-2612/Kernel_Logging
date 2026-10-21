// Character Device Driver: /dev/klog (Major Device 2)
// Enhanced Edition v2.0 - Read/Write Kernel Logging Interface
#include "types.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "mmu.h"
#include "proc.h"
#include "klog.h"

// State tracking for streaming sequential reads
static struct {
  struct spinlock lock;
  uint last_seq_id;
} klog_dev_state;

void
klogdev_init(void)
{
  initlock(&klog_dev_state.lock, "klog_device");
  klog_dev_state.last_seq_id = 0;
  
  // Register read and write handlers in global device switch table
  devsw[KLOG].read  = klogdev_read;
  devsw[KLOG].write = klogdev_write;
}

// Read log records from /dev/klog into user buffer
int
klogdev_read(struct inode *ip, char *dst, int n)
{
  struct klog_entry *staging_page;
  int record_count, idx, total_copied = 0;
  int max_fit;
  struct proc *curproc = myproc();
  
  if(!curproc || !dst || n <= 0)
    return -1;
  
  // Dynamically allocate one staging page to prevent kernel stack overflow
  staging_page = (struct klog_entry*)kalloc();
  if(!staging_page)
    return -1;
  
  max_fit = PGSIZE / sizeof(struct klog_entry);
  if(n / (int)sizeof(struct klog_entry) < max_fit)
    max_fit = n / sizeof(struct klog_entry);
  
  if(max_fit <= 0){
    kfree((char*)staging_page);
    return 0;
  }
  
  record_count = klog_snapshot(staging_page, max_fit);
  
  // Safely stream structured entries into user memory space
  for(idx = 0; idx < record_count && total_copied + sizeof(struct klog_entry) <= (uint)n; idx++){
    if(copyout(curproc->pgdir, (uint)dst + total_copied, &staging_page[idx], sizeof(struct klog_entry)) < 0){
      kfree((char*)staging_page);
      return -1;
    }
    total_copied += sizeof(struct klog_entry);
  }
  
  kfree((char*)staging_page);
  return total_copied;
}

// Write custom event messages into /dev/klog (Unix /dev/kmsg equivalent)
int
klogdev_write(struct inode *ip, char *buf, int n)
{
  char log_payload[KLOG_MSG_MAX];
  int write_len, i;
  int caller_pid = 0;
  
  if(!buf || n <= 0)
    return -1;
  
  if(myproc())
    caller_pid = myproc()->pid;
  
  // Clamp payload length to prevent buffer overruns
  write_len = n < (sizeof(log_payload) - 1) ? n : (sizeof(log_payload) - 1);
  
  for(i = 0; i < write_len; i++){
    char ch = buf[i];
    // Filter non-printable carriage returns and strip trailing newlines
    if(ch == '\n' || ch == '\r')
      break;
    log_payload[i] = ch;
  }
  log_payload[i] = '\0';
  
  // Emit user-space event into kernel ring buffer at standard INFO severity
  klog_printf("user[%d]: %s", caller_pid, log_payload);
  
  return n;
}
