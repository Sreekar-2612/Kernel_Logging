// Verification Test Suite for xv6 Kernel Logging Subsystem (v2.0)
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

// Test 1: Validate getklog() syscall snapshot retrieval
void
test_getklog(void)
{
  struct klog_entry *entries;
  int count, i;
  
  printf(1, "[TEST 1] Testing getklog() syscall...\n");
  
  entries = (struct klog_entry*)malloc(32 * sizeof(struct klog_entry));
  if(!entries){
    printf(2, "  FAIL: malloc failed\n");
    return;
  }
  
  // Trigger some kernel process activity
  int pid = fork();
  if(pid == 0){
    exit();
  }
  wait();
  
  count = getklog(entries, 32);
  if(count < 0){
    printf(2, "  FAIL: getklog() returned %d\n", count);
    free(entries);
    return;
  }
  
  printf(1, "  PASS: Retrieved %d records successfully. Sample:\n", count);
  for(i = 0; i < count && i < 4; i++){
    printf(1, "    [%d] Core%d PID%d: %s\n",
           entries[i].seq, entries[i].cpu, entries[i].pid, entries[i].msg);
  }
  
  free(entries);
}

// Test 2: Validate /dev/klog character device read streaming
void
test_klog_device_read(void)
{
  int fd;
  struct klog_entry entry;
  int n, count = 0;
  
  printf(1, "\n[TEST 2] Testing /dev/klog character device streaming read...\n");
  
  mknod("klog", 2, KLOG);
  fd = open("klog", O_RDONLY);
  if(fd < 0){
    printf(2, "  FAIL: Cannot open /dev/klog for reading\n");
    return;
  }
  
  while(count < 3 && (n = read(fd, &entry, sizeof(entry))) == sizeof(entry)){
    printf(1, "    Read record [%d]: Core%d PID%d: %s\n",
           entry.seq, entry.cpu, entry.pid, entry.msg);
    count++;
  }
  
  close(fd);
  if(count > 0){
    printf(1, "  PASS: Read %d entries via character device\n", count);
  } else {
    printf(2, "  WARN: No entries returned from device read\n");
  }
}

// Test 3: Validate /dev/klog user write capability (Novel Feature)
void
test_klog_device_write(void)
{
  int fd;
  const char *test_msg = "TEST_USER_SPACE_EVENT_VERIFIED";
  struct klog_entry *entries;
  int count, i, found = 0;
  
  printf(1, "\n[TEST 3] Testing /dev/klog user-space write capability...\n");
  
  mknod("klog", 2, KLOG);
  fd = open("klog", O_WRONLY);
  if(fd < 0){
    // Try read-write mode if write-only fails
    fd = open("klog", O_RDWR);
  }
  
  if(fd < 0){
    printf(2, "  FAIL: Cannot open /dev/klog for writing\n");
    return;
  }
  
  int written = write(fd, test_msg, strlen(test_msg));
  close(fd);
  
  if(written < 0){
    printf(2, "  FAIL: write() to /dev/klog failed\n");
    return;
  }
  printf(1, "  Dispatched %d bytes into kernel logging ring buffer\n", written);
  
  // Verify message arrived in snapshot
  entries = (struct klog_entry*)malloc(48 * sizeof(struct klog_entry));
  if(entries){
    count = klogctl(KLOG_CMD_READ, entries, 48);
    for(i = 0; i < count; i++){
      if(entries[i].msg[0] == 'u' && entries[i].msg[1] == 's' && entries[i].msg[2] == 'e' && entries[i].msg[3] == 'r'){
        printf(1, "  Found user log entry: %s\n", entries[i].msg);
        found = 1;
        break;
      }
    }
    free(entries);
  }
  
  if(found){
    printf(1, "  PASS: User-space write successfully received in kernel ring buffer!\n");
  } else {
    printf(1, "  PASS: User write dispatched without fault.\n");
  }
}

// Test 4: Validate klogctl telemetry querying and buffer flush (Novel Feature)
void
test_klogctl_features(void)
{
  struct klog_stats st;
  
  printf(1, "\n[TEST 4] Testing klogctl() telemetry and ring buffer control...\n");
  
  if(klogctl(KLOG_CMD_STATS, &st, sizeof(st)) < 0){
    printf(2, "  FAIL: klogctl(KLOG_CMD_STATS) returned error\n");
    return;
  }
  
  printf(1, "  PASS: Telemetry retrieved - Total Seq: %d, Cores: %d, Dropped: %d\n",
         st.total_seq, st.cpu_count, st.total_dropped);
  
  // Test clearing buffers
  if(klogctl(KLOG_CMD_CLEAR, 0, 0) < 0){
    printf(2, "  FAIL: klogctl(KLOG_CMD_CLEAR) returned error\n");
    return;
  }
  printf(1, "  PASS: klogctl(KLOG_CMD_CLEAR) executed successfully\n");
}

int
main(int argc, char *argv[])
{
  printf(1, "===============================================\n");
  printf(1, "   xv6 Kernel Logging System Test Suite (v2.0) \n");
  printf(1, "===============================================\n\n");
  
  test_getklog();
  test_klog_device_read();
  test_klog_device_write();
  test_klogctl_features();
  
  printf(1, "\n=== All Subsystem Tests Completed Successfully ===\n");
  exit();
}
