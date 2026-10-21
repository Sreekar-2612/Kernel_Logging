# Kernel Event Logging Subsystem for xv6 (Enhanced v2.0)

[![Language](https://img.shields.io/badge/Language-C%20%7C%20x86%20Assembly-blue.svg)](file:///)
[![OS](https://img.shields.io/badge/Platform-xv6%20Unix-orange.svg)](file:///)
[![Status](https://img.shields.io/badge/Status-Production%20Ready-brightgreen.svg)](file:///)

An enterprise-grade, high-performance structured event logging subsystem built directly into the xv6 multi-core operating system kernel.

**Author:** Sreekar Tirumala  
**Email:** sreekartirumala0706@gmail.com

---

## Highlights & Novel Features

- **Per-CPU Ring Buffers:** Zero-lock contention write paths across up to 8 processor cores.
- **Hardware RDTSC Timestamps:** Sub-microsecond event sequencing using CPU cycle counters.
- **Bi-Directional `/dev/klog`:** Supports both streaming reads and interactive user-to-kernel event injection (Unix `/dev/kmsg` equivalent).
- **Control Syscall `klogctl` (Syscall 23):** Snapshot retrieval, telemetry polling, and atomic buffer flushing.
- **Rich CLI Utility (`ulog_tool`):** Multi-level filtering (`-l`), process targeting (`-p`), telemetry auditing (`-s`), buffer flushes (`-c`), and ANSI colorization.
- **Accurate Overwrite Tracking:** Tracks dropped records during heavy log volume bursts.
- **Stack-Safe Staging:** Prevents kernel stack overflow by dynamically paging staging buffers.

---

## Directory Structure

```
Logging_system_in_xv6/
├── README.md               # Project overview and quickstart
├── DEMO.md                 # Interactive walkthrough guide
├── PROJECT_REPORT.md       # Full engineering report and performance analysis
└── xv6-public/             # Complete xv6 operating system source
    ├── klog.h              # Subsystem interface & structures
    ├── klog.c              # Core ring buffer and sequence engine
    ├── klogdev.c           # /dev/klog character device driver
    ├── ulog_tool.c         # User-space CLI inspection tool
    ├── klog_test.c         # Automated verification test suite
    └── ...                 # xv6 kernel and user-space modules
```

---

## Quickstart

### 1. Build and Run xv6
```bash
cd xv6-public
make qemu-nox
```
*(To exit QEMU: press `Ctrl-A` then `X`)*

### 2. Inspect Kernel Logs
```bash
$ ulog_tool
```

### 3. Filter by Severity or Process ID
```bash
$ ulog_tool -l WARN
$ ulog_tool -p 2
```

### 4. Inject User Events into Kernel Log
```bash
$ echo "Application started" > klog
$ ulog_tool -n 3
```

### 5. Check System Telemetry
```bash
$ ulog_tool -s
```

### 6. Run Test Suite
```bash
$ klog_test
```

---

## License & Attribution
Based on MIT's xv6 operating system. Kernel Logging Subsystem v2.0 engineered by Sreekar Tirumala.
