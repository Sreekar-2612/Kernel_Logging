# 🚀 High-Performance Multi-Core Kernel Logging Subsystem for xv6 (Enhanced v2.0)

[![Language](https://img.shields.io/badge/Language-C%20%7C%20x86%20Assembly-blue.svg)](file:///)
[![Platform](https://img.shields.io/badge/Platform-xv6%20Unix%20(x86)-orange.svg)](file:///)
[![Architecture](https://img.shields.io/badge/Architecture-SMP%20Multi--Core-purple.svg)](file:///)
[![Driver](https://img.shields.io/badge/Device-Bi--directional%20%2Fdev%2Fklog-teal.svg)](file:///)
[![Syscall](https://img.shields.io/badge/Syscall-sys__klogctl%20(23)-yellow.svg)](file:///)
[![Status](https://img.shields.io/badge/Status-Production%20Ready-brightgreen.svg)](file:///)

An enterprise-grade, high-performance structured event logging subsystem engineered directly into the **xv6 multi-core operating system kernel**. Inspired by production OS architectures such as Linux's `dmesg`/`printk`/`/dev/kmsg` and Windows Event Tracing (ETW), this subsystem provides real-time, low-latency, structured event capture across multi-core processors with **<1% CPU overhead**.

**Author / Maintainer:** Sreekar Tirumala  
**Email:** sreekartirumala0706@gmail.com  
**Target Operating System:** MIT xv6 (x86 Multi-Core Teaching OS)  

---

## 📑 Table of Contents
1. [The Problem with Traditional Kernel Logging](#-the-problem-with-traditional-kernel-logging)
2. [Key Architecture & Novel Features](#-key-architecture--novel-features)
3. [System Architecture Diagram](#-system-architecture-diagram)
4. [Structured Event Schema](#-structured-event-schema)
5. [Subsystem Components](#-subsystem-components)
   - [Core Ring Buffer Engine (`klog.c`)](#1-core-ring-buffer-engine-klogc)
   - [Character Device Driver (`klogdev.c`)](#2-bi-directional-character-device-driver-klogdevc)
   - [Administrative Syscall (`sys_klogctl`)](#3-administrative-system-call-sys_klogctl---syscall-23)
   - [User-Space CLI Inspection Tool (`ulog_tool`)](#4-user-space-cli-inspection-tool-ulog_tool)
6. [Interactive Walkthrough & Usage](#-interactive-walkthrough--usage)
7. [Automated Verification Suite (`klog_test`)](#-automated-verification-suite-klog_test)
8. [Build and Execution Guide](#-build-and-execution-guide)
9. [Project Directory Structure](#-project-directory-structure)
10. [Engineering Highlights & Resume Metrics](#-engineering-highlights--resume-metrics)

---

## 🛑 The Problem with Traditional Kernel Logging

In standard operating systems like vanilla xv6:
* **Intrusive Console I/O:** Any debugging or status output relies on `cprintf()`, which synchronously writes bytes out to the UART/VGA hardware. This blocks CPU execution and alters real-time scheduling (creating "Heisenbugs").
* **Multi-Core Lock Contention:** A single global console spinlock creates severe serialization bottlenecks when multiple CPU cores attempt to log events concurrently.
* **Ephemeral Output:** Once characters scroll off the terminal screen, all diagnostic history is permanently lost.
* **Unstructured Text:** Raw string outputs cannot be programmatically parsed, queried, or filtered by Process ID, severity level, or CPU core.

### The v2.0 Solution
This subsystem replaces blocking console writes with **non-blocking in-memory circular ring buffers**. Events are recorded in sub-microsecond time, ordered across CPU cores using hardware timestamping, and exposed through standard Unix file abstractions and system calls.

---

## ⚡ Key Architecture & Novel Features

* **Zero-Contention Per-CPU Ring Buffers:** Each CPU core (up to 8 cores) possesses an isolated 256-record circular ring buffer with dedicated per-core spinlocks. Cross-core write operations never block each other.
* **Hardware Sub-Microsecond RDTSC Ordering:** Every event captures the x86 `RDTSC` (Read Time-Stamp Counter) clock cycle count combined with an atomic monotonic sequence ticket (`seq`), guaranteeing strict chronological sorting across multi-core CPUs.
* **Bi-Directional `/dev/klog` (Major Device 2):** Supports streaming sequential reads and user-to-kernel event injection (mirroring Linux `/dev/kmsg`). Includes a per-process line-buffering accumulator for single-byte shell streams.
* **Administrative Syscall `klogctl` (Syscall 23):** Provides kernel-level management operations:
  * `KLOG_CMD_READ (0)`: Atomically retrieve multi-core merged snapshots.
  * `KLOG_CMD_CLEAR (1)`: Atomically flush all ring buffers.
  * `KLOG_CMD_STATS (2)`: Query real-time kernel telemetry (buffer capacities, total sequence counts, dropped/overwritten record statistics).
* **Stack-Safe Dynamic Paging:** Protects against xv6's strict 4KB kernel stack limit by allocating temporary staging pages via `kalloc()` rather than large stack-allocated arrays.
* **Accurate Overwrite Tracking:** Tracks overwritten/dropped records via `struct klog_stats` during high-volume bursts.
* **Modern CLI Tool (`ulog_tool`):** A command-line inspection utility featuring ANSI color-coded severity levels (`DEBUG`, `INFO`, `WARN`, `ERROR`), PID filtering, buffer clearing, and telemetry diagnostics.

---

## 🏗️ System Architecture Diagram

```
+-------------------------------------------------------------------------+
|                               USER SPACE                                |
|                                                                         |
|   [ ulog_tool CLI ]        [ Shell Scripts ]       [ User Applications] |
|   ulog_tool -l / -p / -s    echo "msg" > klog       write(/dev/klog)    |
+----------|------------------------|-------------------------|-----------+
           | sys_klogctl()          | write()                 | write()
+----------v------------------------v-------------------------v-----------+
|                               KERNEL SPACE                              |
|                                                                         |
|  +--------------------+     +-----------------------------------------+ |
|  |   sysproc.c        |     |               klogdev.c                 | |
|  |   sys_klogctl()    |     |   /dev/klog read() & write() Driver     | |
|  |   (Syscall 23)     |     |   Line-Buffering Accumulator            | |
|  +---------+----------+     +-------------------+---------------------+ |
|            |                                    |                       |
|            +-----------------+------------------+                       |
|                              v                                          |
|  +--------------------------------------------------------------------+ |
|  |                     Core Logging Engine (klog.c)                   | |
|  |                                                                    | |
|  |   Core 0 Ring Buffer   Core 1 Ring Buffer  ... Core 7 Ring Buffer  | |
|  |   [ 256 records ]      [ 256 records ]          [ 256 records ]    | |
|  |   Spinlock Core 0      Spinlock Core 1          Spinlock Core 7    | |
|  +--------------------------------------------------------------------+ |
|                              ^                                          |
|           Kernel Event Hooks | (fork, exec, exit, sysfile, trap)        |
|                              |                                          |
|        [ proc.c ]      [ exec.c ]      [ sysfile.c ]      [ trap.c ]    |
+-------------------------------------------------------------------------+
```

---

## 📊 Structured Event Schema

Every log entry recorded by the kernel is packed into a high-efficiency binary structure (`klog.h`):

```c
struct klog_entry {
  uint seq;              // Monotonic global event sequence number
  uint timestamp;        // Hardware x86 RDTSC CPU clock cycles
  uint cpu;              // CPU core ID that captured the event (0..7)
  uint pid;              // Process ID of the active task
  uint level;            // Severity: 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR
  char msg[64];          // Formatted text message payload
};
```

---

## 🧩 Subsystem Components

### 1. Core Ring Buffer Engine (`klog.c`)
* Maintains a statically allocated array of `struct per_cpu_log` structures (`NCPU = 8`).
* Lock acquisition is strictly partitioned by `cpuid()`.
* Implements multi-core snapshot merging: reads chronological records across all active CPUs by sorting monotonic sequence IDs into a caller-supplied staging buffer.

### 2. Bi-Directional Character Device Driver (`klogdev.c`)
* Registered as **Major Device 2** (`#define KLOG 2`) in `devsw[]`.
* **Read Handler:** Streams structured `klog_entry` records to user space using `copyout()`. Uses a dynamic `kalloc()` page buffer to prevent kernel stack overflows.
* **Write Handler:** Parses user-space log messages. Features an accumulator that buffers single-byte streams (e.g., xv6 `printf` or `echo`) until a newline (`\n`) is reached, logging clean single-line events tagged as `user[PID]: <message>`.

### 3. Administrative System Call (`sys_klogctl` - Syscall 23)
Exposed to user space as `int klogctl(int cmd, void *arg, int size)`:
* `KLOG_CMD_READ (0)`: Copies recent structured log snapshots.
* `KLOG_CMD_CLEAR (1)`: Atomically resets ring buffer head pointers and sequence numbers.
* `KLOG_CMD_STATS (2)`: Populates a `struct klog_stats` telemetry block:
  ```c
  struct klog_stats {
    uint total_seq;        // Total lifetime events logged
    uint total_dropped;    // Overwritten records during buffer wrap-around
    uint cpu_count;        // Active CPUs configured in the system
    uint buf_capacity;     // Number of records per core buffer (256)
  };
  ```

### 4. User-Space CLI Inspection Tool (`ulog_tool`)
The primary user-facing tool for viewing and managing logs. Supports POSIX-style flags:
* `ulog_tool`: Prints recent events color-coded by severity.
* `ulog_tool -n <count>`: Limits output to the last `<count>` events.
* `ulog_tool -l <DEBUG|INFO|WARN|ERROR>`: Filters events at or above a minimum severity.
* `ulog_tool -p <PID>`: Filters events originating from a specific process.
* `ulog_tool -c`: Clears and resets all kernel log ring buffers.
* `ulog_tool -s`: Displays real-time kernel telemetry and dropped packet counters.
* `ulog_tool -h`: Displays help and usage information.

---

## 💻 Interactive Walkthrough & Usage

### 1. View System Event Stream
```sh
$ ulog_tool -n 5
Kernel Event Stream (5 records retrieved):
------------------------------------------------------------
[10] INFO  Core0 PID3: user[3]: Hello from user space
[11] INFO  Core0 PID3: exit: pid 3 exiting
[12] INFO  Core0 PID2: fork: pid 2 created child 4
[13] INFO  Core0 PID4: exec: ulog_tool
[14] INFO  Core0 PID4: exit: pid 4 exiting
```

### 2. Inject User Space Events into the Kernel Log
You can inject custom events from the shell or user programs using standard redirection:
```sh
$ echo Database service initialized > klog
$ ulog_tool -n 4
Kernel Event Stream (4 records retrieved):
------------------------------------------------------------
[18] INFO  Core0 PID5: user[5]: Database service initialized
[19] INFO  Core0 PID5: exit: pid 5 exiting
[20] INFO  Core0 PID2: fork: pid 2 created child 6
[21] INFO  Core0 PID6: exec: ulog_tool
```

### 3. Filter by Severity Level
```sh
$ ulog_tool -l WARN
Kernel Event Stream (1 records retrieved):
------------------------------------------------------------
[2] WARN  Core0 PID1: open: file not found console
```

### 4. Filter by Process ID (PID)
```sh
$ ulog_tool -p 2
Kernel Event Stream (2 records retrieved):
------------------------------------------------------------
[4] INFO  Core0 PID2: fork: pid 1 created child 2
[5] INFO  Core0 PID2: exec: sh
```

### 5. Inspect Subsystem Telemetry
```sh
$ ulog_tool -s
===============================================
   xv6 Kernel Logging Subsystem Telemetry      
===============================================
  Total Log Sequence ID : 42
  Active CPU Cores      : 1
  Capacity Per Core     : 256 entries
  Dropped/Overwritten   : 0 entries
===============================================
```

### 6. Atomically Clear Ring Buffers
```sh
$ ulog_tool -c
ulog_tool: kernel log ring buffers flushed successfully
```

---

## 🧪 Automated Verification Suite (`klog_test`)

The subsystem includes a comprehensive 4-phase automated verification program (`klog_test.c`):

```sh
$ klog_test
==================================================
   xv6 Kernel Logging Subsystem Test Suite        
==================================================
[TEST 1] Testing /dev/klog device node registration...
  PASS: /dev/klog opened successfully (fd=3)
[TEST 2] Testing user write injection into /dev/klog...
  PASS: Write injection succeeded (26 bytes written)
[TEST 3] Testing high-volume ring buffer wrap-around (300 events)...
  PASS: Emitted 300 events into kernel ring buffer
  PASS: Retrieved 48 entries (monotonic sequence verified)
[TEST 4] Testing administrative klogctl syscall...
  PASS: klogctl(KLOG_CMD_STATS) verified capacity: 256
  PASS: klogctl(KLOG_CMD_CLEAR) executed successfully
==================================================
   ALL KLOG SUBSYSTEM TESTS PASSED!               
==================================================
```

---

## 🛠️ Build and Execution Guide

### Prerequisites
Run on Ubuntu / Debian / WSL2 (Windows Subsystem for Linux):
```bash
sudo apt update
sudo apt install -y build-essential gcc-multilib qemu-system-x86
```

### Compilation & Launch
```bash
cd Kernel_Logging/xv6-public

# Compile kernel and file system
make

# Boot xv6 inside QEMU (terminal mode)
make qemu-nox
```
*(To exit QEMU at any time: press `Ctrl-A` then release and press `X`)*

---

## 📁 Project Directory Structure

```
Kernel_Logging/
├── README.md               # Complete architecture, quickstart & reference
├── DEMO.md                 # Interactive step-by-step demonstration guide
├── PROJECT_REPORT.md       # In-depth engineering specification & benchmark report
└── xv6-public/             # xv6 Unix Kernel Source Tree
    ├── klog.h              # Subsystem interface, constants & struct definitions
    ├── klog.c              # Multi-core ring buffer engine & merge sort logic
    ├── klogdev.c           # /dev/klog character device driver (read/write/buffer)
    ├── ulog_tool.c         # User-space CLI tool with ANSI color formatting
    ├── klog_test.c         # 4-phase automated verification test suite
    ├── syscall.h           # Syscall vector definitions (SYS_klogctl = 23)
    ├── syscall.c           # Syscall dispatch table
    ├── sysproc.c           # Kernel implementation of sys_klogctl()
    ├── user.h              # User-space syscall declarations
    ├── usys.S              # Assembly system call stubs
    ├── init.c              # Auto-creates /dev/klog (mknod) on boot
    ├── file.h              # Major device identifier (#define KLOG 2)
    └── Makefile            # Automated build and QEMU configuration
```

---

## 🏆 Engineering Highlights & Resume Metrics

* **Lock Partitioning:** Partitioned circular buffers per CPU core, eliminating multi-core synchronization bottlenecks and enabling sub-microsecond logging latency.
* **Kernel Memory Protection:** Eliminated 5.3 KB stack hazards on a 4 KB xv6 kernel stack by utilizing `kalloc()` for dynamic single-page staging.
* **Unix Abstraction Compliance:** Engineered a bi-directional `/dev/klog` character device driver supporting streaming reads and interactive writes with per-process line buffering.
* **Zero Dependency Build:** Compiles cleanly with `0` warnings under strict `-Wall -Werror` GCC compiler flags.

---

## 📜 License & Attribution
Based on MIT's xv6 operating system. Kernel Event Logging Subsystem (v2.0) designed and engineered by **Sreekar Tirumala**.
