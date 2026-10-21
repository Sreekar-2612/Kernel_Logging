// Kernel Logging Subsystem - Public Interface & Structures
// Enhanced Edition v2.0 - Designed for xv6
#ifndef KLOG_H
#define KLOG_H

#include "types.h"
#include "spinlock.h"

// Maximum length of an individual log message payload
#define KLOG_MSG_MAX  64

// Capacity per per-CPU ring buffer (power of 2 for fast bitwise indexing)
#define KLOG_BUF_SIZE 256

// Structured log record (84 bytes, cache-friendly alignment)
struct klog_entry {
  uint seq;           // Monotonically increasing sequence ID
  uint timestamp_hi;  // CPU cycle counter upper 32-bits (RDTSC)
  uint timestamp_lo;  // CPU cycle counter lower 32-bits (RDTSC)
  uint cpu;           // Originating processor hardware ID
  uint pid;           // Originating process ID (0 indicates kernel)
  uint level;         // Severity ranking: DEBUG, INFO, WARN, ERROR
  char msg[KLOG_MSG_MAX]; // Formatted text payload
};

// Telemetry statistics returned to user-space monitoring tools
struct klog_stats {
  uint total_seq;      // Global sequence counter
  uint total_dropped;  // Aggregated dropped/overwritten entries across all CPUs
  uint cpu_count;      // Total CPU cores configured (NCPU)
  uint buf_capacity;   // Ring buffer capacity per core
};

// Per-core ring buffer structure
struct klog_cpu_buf {
  struct spinlock lock;                    // Core-local buffer lock
  struct klog_entry entries[KLOG_BUF_SIZE];// Ring buffer storage
  uint head;                               // Monotonic write index
  uint dropped;                            // Dropped/overwritten entry count
};

// Severity levels
#define KLOG_DEBUG 0
#define KLOG_INFO  1
#define KLOG_WARN  2
#define KLOG_ERROR 3

// Control operations for klogctl syscall
#define KLOG_CMD_READ   0  // Snapshot recent log records into buffer
#define KLOG_CMD_CLEAR  1  // Atomically flush and reset log buffers
#define KLOG_CMD_STATS  2  // Retrieve telemetry statistics structure

// Core kernel interface functions
void klog_init(void);
void klog_printf(const char *fmt, ...);
void klog_printf_level(int level, const char *fmt, ...);
int  klog_snapshot(struct klog_entry *buf, int max_entries);
void klog_clear(void);
uint klog_get_dropped(void);
void klog_get_stats(struct klog_stats *st);

// Streamlined logging convenience macros
#define klog_debug(fmt, ...) klog_printf_level(KLOG_DEBUG, fmt, ##__VA_ARGS__)
#define klog_info(fmt, ...)  klog_printf_level(KLOG_INFO,  fmt, ##__VA_ARGS__)
#define klog_warn(fmt, ...)  klog_printf_level(KLOG_WARN,  fmt, ##__VA_ARGS__)
#define klog_error(fmt, ...) klog_printf_level(KLOG_ERROR, fmt, ##__VA_ARGS__)

#endif // KLOG_H
