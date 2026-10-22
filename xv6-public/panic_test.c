// Diagnostic test utility to demonstrate the Flight Recorder Crash Dump
#include "types.h"
#include "stat.h"
#include "user.h"

int
main(void)
{
  printf(1, "============================================================\n");
  printf(1, "   xv6 Kernel Logging Flight Recorder Crash Diagnostic Test \n");
  printf(1, "============================================================\n");
  printf(1, "[*] Triggering diagnostic kernel panic via sys_klogctl()...\n");
  printf(1, "[*] Expectation: The CPU will halt and the Flight Recorder\n");
  printf(1, "    will dump the last in-flight events before freezing.\n\n");
  
  // Trigger diagnostic panic via syscall
  klogctl(KLOG_CMD_PANIC_TEST, 0, 0);
  
  printf(2, "[-] Error: Kernel panic did not trigger.\n");
  exit();
}
