// User-Space Kernel Log Inspection & Control Utility
// Enhanced Edition v2.0 - Authored for xv6
#include "types.h"
#include "stat.h"
#include "user.h"

#define ANSI_CYAN    "\033[36m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_RED     "\033[1;31m"
#define ANSI_RESET   "\033[0m"

static const char *level_labels[]  = {"DEBUG", "INFO ", "WARN ", "ERROR"};
static const char *level_colors[]  = {ANSI_CYAN, ANSI_GREEN, ANSI_YELLOW, ANSI_RED};
static const char *subsys_labels[] = {"CORE", "PROC", "FS  ", "MEM ", "TRAP", "DEV ", "USER"};

static void
print_usage(void)
{
  printf(1, "Usage: ulog_tool [options]\n");
  printf(1, "Options:\n");
  printf(1, "  -h          Display this help message\n");
  printf(1, "  -l <LEVEL>  Filter by minimum severity (DEBUG, INFO, WARN, ERROR)\n");
  printf(1, "  -p <PID>    Filter events originating from specific process ID\n");
  printf(1, "  -t <SUBSYS> Filter by subsystem (CORE, PROC, FS, MEM, TRAP, DEV, USER)\n");
  printf(1, "  -c          Clear and flush all kernel ring buffers\n");
  printf(1, "  -s          Display kernel logging subsystem telemetry and metrics\n");
  printf(1, "  -n <COUNT>  Limit output to the last <COUNT> records\n");
}

static int
parse_level_str(const char *s)
{
  if(strcmp(s, "DEBUG") == 0 || strcmp(s, "debug") == 0 || strcmp(s, "0") == 0)
    return 0;
  if(strcmp(s, "INFO") == 0  || strcmp(s, "info") == 0  || strcmp(s, "1") == 0)
    return 1;
  if(strcmp(s, "WARN") == 0  || strcmp(s, "warn") == 0  || strcmp(s, "2") == 0)
    return 2;
  if(strcmp(s, "ERROR") == 0 || strcmp(s, "error") == 0 || strcmp(s, "3") == 0)
    return 3;
  return -1;
}

static int
parse_subsys_str(const char *s)
{
  if(strcmp(s, "CORE") == 0 || strcmp(s, "core") == 0 || strcmp(s, "0") == 0)
    return 0;
  if(strcmp(s, "PROC") == 0 || strcmp(s, "proc") == 0 || strcmp(s, "1") == 0)
    return 1;
  if(strcmp(s, "FS") == 0   || strcmp(s, "fs") == 0   || strcmp(s, "2") == 0)
    return 2;
  if(strcmp(s, "MEM") == 0  || strcmp(s, "mem") == 0  || strcmp(s, "3") == 0)
    return 3;
  if(strcmp(s, "TRAP") == 0 || strcmp(s, "trap") == 0 || strcmp(s, "4") == 0)
    return 4;
  if(strcmp(s, "DEV") == 0  || strcmp(s, "dev") == 0  || strcmp(s, "5") == 0)
    return 5;
  if(strcmp(s, "USER") == 0 || strcmp(s, "user") == 0 || strcmp(s, "6") == 0)
    return 6;
  return -1;
}

int
main(int argc, char *argv[])
{
  int min_level = 0;       // Default: show all levels
  int target_pid = -1;     // Default: all PIDs
  int target_subsys = -1;  // Default: all subsystems
  int limit_count = 64;    // Default snapshot capacity
  int clear_flag = 0;
  int stats_flag = 0;
  int i, count, displayed;
  struct klog_entry *entries;
  
  // Parse command line flags
  for(i = 1; i < argc; i++){
    if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0){
      print_usage();
      exit();
    } else if(strcmp(argv[i], "-c") == 0){
      clear_flag = 1;
    } else if(strcmp(argv[i], "-s") == 0){
      stats_flag = 1;
    } else if(strcmp(argv[i], "-l") == 0){
      if(i + 1 < argc){
        min_level = parse_level_str(argv[++i]);
        if(min_level < 0){
          printf(2, "Invalid log level: %s (choose DEBUG, INFO, WARN, ERROR)\n", argv[i]);
          exit();
        }
      } else {
        printf(2, "Missing argument for -l\n");
        exit();
      }
    } else if(strcmp(argv[i], "-p") == 0){
      if(i + 1 < argc){
        target_pid = atoi(argv[++i]);
      } else {
        printf(2, "Missing argument for -p\n");
        exit();
      }
    } else if(strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--subsys") == 0){
      if(i + 1 < argc){
        target_subsys = parse_subsys_str(argv[++i]);
        if(target_subsys < 0){
          printf(2, "Invalid subsystem: %s (choose CORE, PROC, FS, MEM, TRAP, DEV, USER)\n", argv[i]);
          exit();
        }
      } else {
        printf(2, "Missing argument for -t\n");
        exit();
      }
    } else if(strcmp(argv[i], "-n") == 0){
      if(i + 1 < argc){
        limit_count = atoi(argv[++i]);
        if(limit_count <= 0)
          limit_count = 64;
      } else {
        printf(2, "Missing argument for -n\n");
        exit();
      }
    } else {
      printf(2, "Unknown option: %s (run 'ulog_tool -h' for usage)\n", argv[i]);
      exit();
    }
  }
  
  // Handle clear request
  if(clear_flag){
    if(klogctl(KLOG_CMD_CLEAR, 0, 0) < 0){
      printf(2, "ulog_tool: failed to clear kernel log buffer\n");
      exit();
    }
    printf(1, "[+] Kernel log ring buffers cleared successfully.\n");
    exit();
  }
  
  // Handle stats request
  if(stats_flag){
    struct klog_stats st;
    if(klogctl(KLOG_CMD_STATS, &st, sizeof(st)) < 0){
      printf(2, "ulog_tool: failed to query kernel log telemetry\n");
      exit();
    }
    printf(1, "===============================================\n");
    printf(1, "   xv6 Kernel Logging Subsystem Telemetry      \n");
    printf(1, "===============================================\n");
    printf(1, "  Total Log Sequence ID : %d\n", st.total_seq);
    printf(1, "  Active CPU Cores      : %d\n", st.cpu_count);
    printf(1, "  Capacity Per Core     : %d entries\n", st.buf_capacity);
    printf(1, "  Dropped/Overwritten   : %d entries\n", st.total_dropped);
    printf(1, "===============================================\n");
    exit();
  }
  
  // Allocate heap buffer for log entries
  entries = (struct klog_entry*)malloc(limit_count * sizeof(struct klog_entry));
  if(!entries){
    printf(2, "ulog_tool: memory allocation failed\n");
    exit();
  }
  
  // Fetch snapshot via klogctl (falls back to getklog if needed)
  count = klogctl(KLOG_CMD_READ, entries, limit_count);
  if(count < 0){
    // Fallback to legacy getklog
    count = getklog(entries, limit_count);
  }
  
  if(count < 0){
    printf(2, "ulog_tool: failed to retrieve log snapshot from kernel\n");
    free(entries);
    exit();
  }
  
  printf(1, "Kernel Event Stream (%d records retrieved):\n", count);
  printf(1, "------------------------------------------------------------\n");
  
  displayed = 0;
  for(i = 0; i < count; i++){
    // Apply user filters
    if(entries[i].level < (uint)min_level)
      continue;
    if(target_pid >= 0 && entries[i].pid != (uint)target_pid)
      continue;
    if(target_subsys >= 0 && entries[i].subsys != (uint)target_subsys)
      continue;
    
    const char *subsys_str = (entries[i].subsys < 7) ? subsys_labels[entries[i].subsys] : "UNKN";
    
    if(entries[i].level < 4){
      printf(1, "[%d] %s%s%s [%s] Core%d PID%d: %s\n",
             entries[i].seq,
             level_colors[entries[i].level],
             level_labels[entries[i].level],
             ANSI_RESET,
             subsys_str,
             entries[i].cpu,
             entries[i].pid,
             entries[i].msg);
    } else {
      printf(1, "[%d] ??? [%s] Core%d PID%d: %s\n",
             entries[i].seq,
             subsys_str,
             entries[i].cpu,
             entries[i].pid,
             entries[i].msg);
    }
    displayed++;
  }
  
  if(displayed == 0 && count > 0){
    printf(1, "(No log records matched the specified filter criteria)\n");
  }
  
  free(entries);
  exit();
}
