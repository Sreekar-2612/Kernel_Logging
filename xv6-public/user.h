struct stat;
struct rtcdate;

// system calls
int fork(void);
int exit(void) __attribute__((noreturn));
int wait(void);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(char*, char**);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
int chdir(const char*);
int dup(int);
int getpid(void);
char* sbrk(int);
int sleep(int);
int uptime(void);

// Kernel logging subsystem (v2.0 Enhanced)
#define KLOG_SUBSYS_CORE  0
#define KLOG_SUBSYS_PROC  1
#define KLOG_SUBSYS_FS    2
#define KLOG_SUBSYS_MEM   3
#define KLOG_SUBSYS_TRAP  4
#define KLOG_SUBSYS_DEV   5
#define KLOG_SUBSYS_USER  6
#define KLOG_SUBSYS_COUNT 7

struct klog_entry {
  unsigned int seq;
  unsigned int timestamp_hi;
  unsigned int timestamp_lo;
  unsigned int cpu;
  unsigned int pid;
  unsigned int level;
  unsigned int subsys;
  char msg[64];
};

struct klog_stats {
  unsigned int total_seq;
  unsigned int total_dropped;
  unsigned int cpu_count;
  unsigned int buf_capacity;
};

#define KLOG_CMD_READ        0
#define KLOG_CMD_CLEAR       1
#define KLOG_CMD_STATS       2
#define KLOG_CMD_PANIC_TEST  3

int getklog(struct klog_entry*, int);
int klogctl(int, void*, int);

// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*);
void *memmove(void*, const void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void printf(int, const char*, ...);
char* gets(char*, int max);
uint strlen(const char*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);
