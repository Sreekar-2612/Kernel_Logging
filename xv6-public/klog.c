// Kernel Logging Subsystem - Core Engine Implementation
// Enhanced Edition v2.0 - Designed for xv6
#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "klog.h"

// Ring buffers partitioned per processor core to eliminate write contention
static struct klog_cpu_buf cpu_logs[NCPU];

// Global sequence generator ensuring total event ordering across all cores
static uint global_seq = 0;
static struct spinlock seq_lock;

// Capture high-precision hardware timestamp using x86 Time Stamp Counter
static inline void
klog_read_tsc(uint *hi, uint *lo)
{
  uint tsc_lo, tsc_hi;
  asm volatile("rdtsc" : "=a"(tsc_lo), "=d"(tsc_hi));
  *hi = tsc_hi;
  *lo = tsc_lo;
}

// Atomically allocate the next global sequence ticket
static uint
klog_next_seq(void)
{
  uint seq;
  acquire(&seq_lock);
  seq = global_seq++;
  release(&seq_lock);
  return seq;
}

// Initialize kernel logging data structures and per-CPU spinlocks
void
klog_init(void)
{
  int cpu_idx;
  
  initlock(&seq_lock, "klog_seq_lock");
  
  for(cpu_idx = 0; cpu_idx < NCPU; cpu_idx++){
    initlock(&cpu_logs[cpu_idx].lock, "klog_core_lock");
    cpu_logs[cpu_idx].head = 0;
    cpu_logs[cpu_idx].dropped = 0;
  }
  
  klog_printf("klog: kernel event logging subsystem initialized (v2.0)");
}

// Format a decimal integer into the target buffer safely
static int
klog_fmt_int(char *buf, int size, int val)
{
  char digit_stack[16];
  int depth = 0, written = 0;
  int is_negative = 0;
  
  if(size <= 0)
    return 0;
  
  if(val < 0){
    is_negative = 1;
    val = -val;
  }
  
  if(val == 0){
    digit_stack[depth++] = '0';
  } else {
    while(val > 0 && depth < sizeof(digit_stack)){
      digit_stack[depth++] = '0' + (val % 10);
      val /= 10;
    }
  }
  
  if(is_negative && written < size - 1)
    buf[written++] = '-';
  
  while(depth > 0 && written < size - 1)
    buf[written++] = digit_stack[--depth];
  
  buf[written] = '\0';
  return written;
}

// Format a hexadecimal integer with '0x' prefix
static int
klog_fmt_hex(char *buf, int size, uint val)
{
  char hex_stack[16];
  int depth = 0, written = 0;
  static const char hex_digits[] = "0123456789abcdef";
  
  if(size <= 2)
    return 0;
  
  buf[written++] = '0';
  buf[written++] = 'x';
  
  if(val == 0){
    hex_stack[depth++] = '0';
  } else {
    while(val > 0 && depth < sizeof(hex_stack)){
      hex_stack[depth++] = hex_digits[val & 0xf];
      val >>= 4;
    }
  }
  
  while(depth > 0 && written < size - 1)
    buf[written++] = hex_stack[--depth];
  
  buf[written] = '\0';
  return written;
}

// Core formatting and ring-buffer dispatch routine
static void
klog_emit_entry(int level, const char *fmt, uint *args)
{
  int cpu_id;
  struct klog_cpu_buf *log;
  struct klog_entry *entry;
  uint slot_idx;
  int pid = 0;
  char line_buf[KLOG_MSG_MAX];
  int cursor = 0;
  char *str_arg;
  
  // Protect CPU context and fetch local core ID
  pushcli();
  cpu_id = cpuid();
  if(cpu_id < 0 || cpu_id >= NCPU){
    popcli();
    return;
  }
  
  log = &cpu_logs[cpu_id];
  
  // Track originating process if invoked from process context
  if(myproc())
    pid = myproc()->pid;
  
  // Format message payload safely into line_buf
  for(; *fmt && cursor < sizeof(line_buf) - 1; fmt++){
    if(*fmt != '%'){
      line_buf[cursor++] = *fmt;
      continue;
    }
    fmt++;
    if(*fmt == '\0')
      break;
    
    switch(*fmt){
    case 'd':
      cursor += klog_fmt_int(line_buf + cursor, sizeof(line_buf) - cursor, *args++);
      break;
    case 'x':
      cursor += klog_fmt_hex(line_buf + cursor, sizeof(line_buf) - cursor, *args++);
      break;
    case 's':
      str_arg = (char*)*args++;
      if(!str_arg)
        str_arg = "(null)";
      for(; *str_arg && cursor < sizeof(line_buf) - 1; str_arg++)
        line_buf[cursor++] = *str_arg;
      break;
    case '%':
      line_buf[cursor++] = '%';
      break;
    default:
      line_buf[cursor++] = '%';
      line_buf[cursor++] = *fmt;
      break;
    }
  }
  line_buf[cursor] = '\0';
  
  // Insert record into core-local circular ring buffer
  acquire(&log->lock);
  
  // Overwrite tracking: increment dropped count if buffer has wrapped around
  if(log->head >= KLOG_BUF_SIZE){
    log->dropped++;
  }
  
  slot_idx = log->head % KLOG_BUF_SIZE;
  entry = &log->entries[slot_idx];
  
  entry->seq = klog_next_seq();
  klog_read_tsc(&entry->timestamp_hi, &entry->timestamp_lo);
  entry->cpu = cpu_id;
  entry->pid = pid;
  entry->level = level;
  
  // Safe payload copy
  for(cursor = 0; cursor < sizeof(entry->msg) - 1 && line_buf[cursor]; cursor++)
    entry->msg[cursor] = line_buf[cursor];
  entry->msg[cursor] = '\0';
  
  log->head++;
  
  release(&log->lock);
  popcli();
}

// Log a formatted entry at standard INFO severity
void
klog_printf(const char *fmt, ...)
{
  uint *args = (uint*)(void*)&fmt + 1;
  klog_emit_entry(KLOG_INFO, fmt, args);
}

// Log a formatted entry at specified severity level
void
klog_printf_level(int level, const char *fmt, ...)
{
  uint *args = (uint*)(void*)&fmt + 1;
  klog_emit_entry(level, fmt, args);
}

// Snapshot the most recent kernel log records across all processor buffers
int
klog_snapshot(struct klog_entry *out_buf, int max_records)
{
  int cpu_id, copied = 0, total_available = 0;
  struct klog_cpu_buf *log;
  uint start_idx, end_idx, iter, slot, i, j;
  uint lowest_seq = 0xFFFFFFFF;
  int skip_excess = 0;
  
  if(!out_buf || max_records <= 0)
    return 0;
  
  // Phase 1: Determine total records stored and compute earliest available sequence ID
  for(cpu_id = 0; cpu_id < NCPU; cpu_id++){
    log = &cpu_logs[cpu_id];
    acquire(&log->lock);
    
    if(log->head > KLOG_BUF_SIZE){
      total_available += KLOG_BUF_SIZE;
      for(iter = 0; iter < KLOG_BUF_SIZE; iter++){
        if(log->entries[iter].seq < lowest_seq)
          lowest_seq = log->entries[iter].seq;
      }
    } else {
      total_available += log->head;
      for(iter = 0; iter < log->head; iter++){
        if(log->entries[iter].seq < lowest_seq)
          lowest_seq = log->entries[iter].seq;
      }
    }
    
    release(&log->lock);
  }
  
  // Calculate window to retain only the newest records if capacity is exceeded
  if(total_available > max_records){
    skip_excess = total_available - max_records;
  }
  
  // Phase 2: Extract qualifying records into destination buffer
  for(cpu_id = 0; cpu_id < NCPU; cpu_id++){
    log = &cpu_logs[cpu_id];
    acquire(&log->lock);
    
    if(log->head > KLOG_BUF_SIZE){
      start_idx = log->head - KLOG_BUF_SIZE;
      end_idx = log->head;
    } else {
      start_idx = 0;
      end_idx = log->head;
    }
    
    for(iter = start_idx; iter < end_idx && copied < max_records; iter++){
      slot = iter % KLOG_BUF_SIZE;
      if(log->entries[slot].seq >= lowest_seq + skip_excess){
        out_buf[copied++] = log->entries[slot];
      }
    }
    
    release(&log->lock);
  }
  
  // Phase 3: Sort collected snapshot chronologically by global sequence ID
  for(i = 0; i < copied - 1; i++){
    for(j = 0; j < copied - i - 1; j++){
      if(out_buf[j].seq > out_buf[j+1].seq){
        struct klog_entry temp_record = out_buf[j];
        out_buf[j] = out_buf[j+1];
        out_buf[j+1] = temp_record;
      }
    }
  }
  
  return copied;
}

// Atomically flush and reset all ring buffers across all CPU cores
void
klog_clear(void)
{
  int cpu_id;
  struct klog_cpu_buf *log;
  
  for(cpu_id = 0; cpu_id < NCPU; cpu_id++){
    log = &cpu_logs[cpu_id];
    acquire(&log->lock);
    log->head = 0;
    log->dropped = 0;
    release(&log->lock);
  }
  
  klog_printf("klog: ring buffers flushed by administrator");
}

// Return the aggregate count of overwritten/dropped log entries
uint
klog_get_dropped(void)
{
  int cpu_id;
  uint total_dropped = 0;
  struct klog_cpu_buf *log;
  
  for(cpu_id = 0; cpu_id < NCPU; cpu_id++){
    log = &cpu_logs[cpu_id];
    acquire(&log->lock);
    total_dropped += log->dropped;
    release(&log->lock);
  }
  
  return total_dropped;
}

// Populate telemetry statistics for administrative inspection
void
klog_get_stats(struct klog_stats *st)
{
  int cpu_id;
  uint dropped_count = 0;
  
  if(!st)
    return;
  
  acquire(&seq_lock);
  st->total_seq = global_seq;
  release(&seq_lock);
  
  for(cpu_id = 0; cpu_id < NCPU; cpu_id++){
    acquire(&cpu_logs[cpu_id].lock);
    dropped_count += cpu_logs[cpu_id].dropped;
    release(&cpu_logs[cpu_id].lock);
  }
  
  st->total_dropped = dropped_count;
  st->cpu_count = NCPU;
  st->buf_capacity = KLOG_BUF_SIZE;
}
