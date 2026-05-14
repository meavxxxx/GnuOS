# GNU OS Development Roadmap

---

## Contents

- [Project Concept](#project-concept)
- [System Architecture](#system-architecture)
- [Development Phases](#development-phases)
  - [Phase 0 - Preparation and Infrastructure](#phase-0---preparation-and-infrastructure)
  - [Phase 1 - Kernel and Bootloader](#phase-1---kernel-and-bootloader)
  - [Phase 2 - System Libraries and ABI](#phase-2---system-libraries-and-abi)
  - [Phase 3 - GNU Toolchain and Userland](#phase-3---gnu-toolchain-and-userland)
  - [Phase 4 - Filesystems and Storage](#phase-4---filesystems-and-storage)
  - [Phase 5 - Networking and IPC](#phase-5---networking-and-ipc)
  - [Phase 6 - User Environment](#phase-6---user-environment)
  - [Phase 7 - Package Manager and Ecosystem](#phase-7---package-manager-and-ecosystem)
  - [Phase 8 - Graphics Subsystem](#phase-8---graphics-subsystem)
  - [Phase 9 - Security and Auditing](#phase-9---security-and-auditing)
  - [Phase 10 - Stabilization and Release](#phase-10---stabilization-and-release)
- [Technology Stack](#technology-stack)
- [Repository Structure](#repository-structure)
- [Development Standards](#development-standards)
- [Build System](#build-system)
- [Contributing](#contributing)
- [Licensing](#licensing)
- [Glossary](#glossary)

---

## Project Concept

### Mission

Build a fully free operating system with POSIX alignment, a custom microkernel,
and a full userland based on GNU tooling.

### Principles

| Principle | Description |
|---|---|
| **Freedom** | Project code is distributed under GPLv3 or compatible licenses |
| **Transparency** | Open development, public issue tracking, public RFCs |
| **Reliability** | Stable ABI and formal verification for critical modules |
| **POSIX Alignment** | Targeting POSIX.1-2017 (IEEE Std 1003.1) behavior |
| **Modularity** | Microkernel architecture with isolated components |

### Target Platforms

- `x86_64` - primary architecture
- `aarch64` (ARM64) - secondary architecture
- `riscv64` - experimental architecture

---

## System Architecture

```text
+-------------------------------------------------------------+
|                    User Applications                         |
|       (bash, coreutils, editors, compilers, ...)            |
+-------------------------------------------------------------+
|                     GNU C Library (glibc)                   |
|                    Syscalls / POSIX API                     |
+----------------+----------------+----------------+----------+
|   VFS Layer    |   Net Stack    |  Device Layer  | IPC/RPC  |
+----------------+----------------+----------------+----------+
|                 User-space Servers                           |
|         (filesystem servers, drivers, init)                 |
+-------------------------------------------------------------+
|                    MICROKERNEL (Ring 0)                     |
|  Scheduler | Memory Management | IPC | Interrupt Handling   |
+-------------------------------------------------------------+
|                  GNU GRUB / Bootloader                      |
+-------------------------------------------------------------+
|                         HARDWARE                            |
+-------------------------------------------------------------+
```

**Kernel type:** Microkernel (inspired by GNU Hurd / L4)  
**Security model:** Capability-based security  
**ABI:** System V AMD64 ABI (`x86_64`), AAPCS64 (`aarch64`)

---

## Development Phases

---

### Phase 0 - Preparation and Infrastructure

**Timeline:** Months 1-2  
**Status:** 🟡 In progress

#### 0.1 Organizational Infrastructure

- [x] Create a git monorepo with branch policies
- [x] Configure CI/CD pipeline (builds, unit tests, integration tests)
- [x] Public issue tracker
- [x] RFC (Request for Comments) process for architecture decisions
- [x] Code of Conduct
- [x] Contributor guide (`CONTRIBUTING.md`)
- [x] GNU Mailman setup

#### 0.2 Cross-Compiler and Build Environment

- [x] Build `binutils` cross-toolchain for target architectures
- [x] Build `GCC` cross-compiler (C, C++) for `x86_64-gnuos-elf`
- [x] Configure `QEMU` for all target platforms
- [x] Docker image / container for reproducible builds
- [x] Automated developer environment setup scripts

#### 0.3 Quality Tooling

- [x] Static analysis (cppcheck, clang-tidy)
- [x] Code formatting (`clang-format`, `.editorconfig`)
- [x] Code coverage (gcov / lcov)
- [x] Fuzzing infrastructure (AFL++, libFuzzer)
- [x] Review policy: minimum 2 approvals before merge

#### 0.4 Documentation

- [x] Architecture Decision Records (ADR)
- [x] Coding standards (GNU Coding Standards + extensions)
- [x] Issue and pull request templates
- [x] Wiki with glossary and FAQ

---

### Phase 1 - Kernel and Bootloader

**Timeline:** Months 3-8  
**Status:** 🟡 In progress  
**Dependencies:** Phase 0

#### 1.1 Bootloader

- [x] Multiboot2 protocol support (GRUB 2 compatible)
- [x] GDT setup for `x86_64`
- [x] Transition to 64-bit long mode
- [x] Basic stack initialization
- [x] Transfer control to kernel with memory map
- [x] UEFI support (UEFI stub loader)
- [x] Minimal UEFI stub scaffolding (`BOOTX64.EFI`, OVMF run script, prototype handoff path in `kmain`)
- [x] Early framebuffer setup (VESA / GOP)

#### 1.2 Early Kernel Initialization

- [x] Early physical page allocator bring-up
- [x] IDT (Interrupt Descriptor Table) setup
- [x] CPU exception handlers (page fault, GPF, #DE, ...)
- [x] APIC / xAPIC / x2APIC setup
- [x] Basic VGA/serial debug output (`kprintf`)
- [x] ACPI initialization (RSDP, MADT, FADT parsing)
- [x] HPET / APIC timer initialization

#### 1.3 Physical Memory Management

- [x] Buddy physical page allocator
- [x] E820 / UEFI memory map parsing
- [x] NUMA (Non-Uniform Memory Access) support
- [x] Reserved memory regions for DMA, MMIO, ACPI
- [ ] Memory usage statistics (`/proc/meminfo`-compatible)

#### 1.4 Virtual Memory Manager (VMM)

- [x] 4-level page tables (PML4) for `x86_64`
- [ ] Kernel virtual address space layout (KASLR-ready)
- [x] Virtual address space allocator
- [x] Huge pages support (2 MB, 1 GB)
- [ ] Copy-on-Write (CoW) for `fork()`
- [ ] Demand paging
- [ ] `mmap()` / `munmap()` / `mprotect()`
- [ ] SMEP / SMAP / NX support

#### 1.5 Process Scheduler

- [x] Core structures: TCB, PCB
- [ ] Preemptive multitasking
- [ ] CFS (Completely Fair Scheduler)
- [ ] Scheduling priorities and classes (`SCHED_FIFO`, `SCHED_RR`, `SCHED_OTHER`)
- [ ] SMP load balancing across CPUs
- [ ] Kernel/user context save and restore
- [ ] Scheduler timer (tick-based -> tickless)
- [ ] Per-CPU idle task
- [ ] `/proc/[pid]/stat`-compatible stats

#### 1.6 Process Management

- [ ] `fork()` / `vfork()` / `clone()`
- [ ] `exec*()` family (`execve`, `execvp`, ...)
- [ ] `wait()` / `waitpid()` / `waitid()`
- [ ] `exit()` / `_exit()` with proper resource cleanup
- [ ] POSIX signals (`kill`, `sigaction`, `sigprocmask`, ...)
- [ ] Process groups, sessions, controlling terminal
- [ ] Namespaces: PID, mount, UTS, IPC, net

#### 1.7 Kernel Threads and Synchronization

- [x] Kernel threads
- [x] Mutexes, spinlocks, RW locks
- [x] Atomic operations (`__atomic_*`)
- [x] Wait queues
- [x] Work queues for deferred jobs
- [x] RCU (Read-Copy-Update)

#### 1.8 Microkernel IPC

- [x] Synchronous message passing (rendezvous)
- [x] Asynchronous message queues
- [x] Capability transfer (FDs, rights)
- [x] Shared memory between processes (SHM)
- [x] Named kernel channels

#### 1.9 Low-Level Drivers

- [x] Serial port (UART 16550) debug output
- [x] PS/2 keyboard (basic)
- [x] Keyboard input buffer and scancode -> ASCII translation
- [x] PCI / PCIe bus scan and enumeration
- [x] MSI / MSI-X interrupts
- [x] Basic DMA engine

---

### Phase 2 - System Libraries and ABI

**Timeline:** Months 7-12  
**Status:** 🔲 Not started  
**Dependencies:** Phase 1.1-1.6

#### 2.1 Syscall Layer

- [x] Syscall table (`syscall_table`) with Linux-compatible numbering (optional)
- [x] `syscall`/`sysret` entry and exit (`x86_64` fast path)
- [x] User pointer validation before dereference
- [x] Syscall auditing (seccomp-compatible filter)
- [x] Tests for each syscall

#### 2.2 GNU C Library (glibc) Port

- [x] Port glibc for `gnuos`
- [x] Implement start files (`crt0.S`, `crti.S`, `crtn.S`)
- [x] Dynamic linking support (`ld.so` / `PT_INTERP`)
- [ ] POSIX threads (`pthreads`) on top of `clone()`
- [x] Minimal pthread ABI scaffolding (`<pthread.h>`, `pthread_*`/`pthread_attr_*` stubs)
- [x] Minimal POSIX semaphore ABI scaffolding (`<semaphore.h>`, `sem_*` stubs)
- [x] Minimal signal ABI scaffolding (`<signal.h>`, `sig*`/`kill`/`raise` stubs)
- [x] Minimal socket ABI scaffolding (`<sys/socket.h>`, `<netinet/in.h>`, `<arpa/inet.h>`)
- [x] Minimal file I/O ABI scaffolding (`<fcntl.h>`, `<sys/stat.h>`, `read`/`write`/...)
- [x] Minimal process/cwd ABI scaffolding (`getpid`/`getuid`/`getgid`/`chdir`/`getcwd`)
- [x] Minimal memory mapping ABI scaffolding (`<sys/mman.h>`, `mmap`/`munmap`/`mprotect`)
- [x] Minimal errno ABI scaffolding (`<errno.h>`, `__errno_location`)
- [x] POSIX-compatible error returns in stage0 stubs (`-1`/`MAP_FAILED` + `errno`) for `sem*`, `sig*`, socket/file/mmap APIs
- [x] TLS support (`%fs` base)
- [x] Implement `dl_iterate_phdr`, `backtrace()`

#### 2.3 Dynamic Loader

- [x] ELF loader (PT_LOAD, PT_DYNAMIC, PT_GNU_RELRO parsing)
- [x] Symbol resolution (PLT / GOT)
- [x] Init ordering: `.init_array`, `DT_INIT`
- [x] `dlopen()` / `dlsym()` / `dlclose()`
- [x] `LD_PRELOAD` mechanism
- [ ] ASLR for shared libraries

#### 2.4 System Math Library

- [ ] Port `libm` (GNU libm or CORE-MATH)
- [ ] IEEE 754 support (rounding modes, NaN, Inf)
- [ ] Hardware acceleration (SSE2, AVX for `x86_64`)

#### 2.5 Standard Headers and POSIX Compatibility

- [ ] Full `<unistd.h>`, `<sys/types.h>`, `<sys/stat.h>`, ... set
- [ ] `<signal.h>` with full POSIX signal set
- [ ] `<pthread.h>` with extensions (`pthread_attr_*`, `sem_*`, ...)
- [ ] `<sys/socket.h>`, `<netinet/in.h>`, `<arpa/inet.h>`
- [x] Minimal network ABI scaffolding (`socket`/`bind`/`connect` + `hton*`/`inet_*`)
- [x] Minimal file ABI scaffolding (`<fcntl.h>`, `<sys/stat.h>`, `<unistd.h>`)
- [x] Minimal process/cwd ABI scaffolding (`getpid`/`getuid`/`getgid`/`chdir`/`getcwd`)
- [x] Minimal `mmap` ABI scaffolding (`<sys/mman.h>`, `mmap`/`munmap`/`mprotect`)
- [x] Minimal POSIX error compatibility in stage0 `libc.so.6` stub (`-1`/`MAP_FAILED` + `errno`)
- [ ] POSIX conformance test suite (POSIX Test Suite / LTP)

---

### Phase 3 - GNU Toolchain and Userland

**Timeline:** Months 10-16  
**Status:** 🔲 Not started  
**Dependencies:** Phase 2

#### 3.1 GNU Coreutils Port

- [ ] Base utilities: `ls`, `cp`, `mv`, `rm`, `mkdir`, `rmdir`
- [ ] Text utilities: `cat`, `echo`, `printf`, `tee`, `head`, `tail`
- [ ] Sorting/filtering: `sort`, `uniq`, `wc`, `grep`, `sed`, `awk`
- [ ] Permission tools: `chmod`, `chown`, `chgrp`, `umask`
- [ ] System info: `uname`, `hostname`, `uptime`, `df`, `du`
- [ ] Process tools: `ps`, `kill`, `nice`, `nohup`

#### 3.2 GNU Bash

- [ ] Port GNU Bash (latest stable)
- [ ] Interactive mode (readline, history, tab completion)
- [ ] Script mode (full POSIX sh compatibility)
- [ ] Job control (`fg`, `bg`, `jobs`, `wait`)
- [ ] Environment variables and export

#### 3.3 GNU Binutils

- [ ] `as` for `x86_64`, `aarch64`
- [ ] `ld` with ELF and linker script support
- [ ] `objdump`, `readelf`, `nm`, `strings`, `strip`
- [ ] `ar`, `ranlib` for static archives
- [ ] `addr2line` symbol/address decoding

#### 3.4 GCC (GNU Compiler Collection)

- [ ] Port GCC for `x86_64-gnuos`
- [ ] C11, C17, C23 support
- [ ] C++17, C++20 support
- [ ] LTO (Link-Time Optimization)
- [ ] Sanitizers: ASan, UBSan, TSan
- [ ] Profiling: `gprof`, `gcov`

#### 3.5 Build Tools

- [ ] Port GNU Make
- [ ] Port Autoconf / Automake / Libtool
- [ ] `pkg-config` support
- [ ] CMake support (optional)

#### 3.6 Editors and Text Tools

- [ ] GNU nano
- [ ] GNU Emacs
- [ ] `less`, `more`
- [ ] `diff`, `patch`

---

### Phase 4 - Filesystems and Storage

**Timeline:** Months 9-15  
**Status:** 🔲 Not started  
**Dependencies:** Phase 1.4

#### 4.1 VFS (Virtual File System)

- [ ] VFS abstractions: inode, dentry, file, superblock
- [ ] Inode and dentry caches
- [ ] Page cache integration with VMM
- [ ] Unified buffer cache
- [ ] `mount` / `umount` / `pivot_root`
- [ ] Bind mounts, overlay mounts
- [ ] Pseudo filesystems: `/proc`, `/sys`, `/dev`

#### 4.2 Core Filesystems

- [ ] **ext4**: full read/write, journaling, extents
- [ ] **tmpfs**: in-memory FS (for `/tmp`, `/run`)
- [ ] **devtmpfs**: automatic device nodes
- [ ] **procfs** (`/proc`): process information
- [ ] **sysfs** (`/sys`): kernel device tree
- [ ] **FAT32 / exFAT**: removable media compatibility

#### 4.3 Additional Filesystems (optional)

- [ ] **Btrfs**: CoW, snapshots, RAID
- [ ] **XFS**: high-performance filesystem
- [ ] **ISO 9660 / UDF** optical media support
- [ ] **NFS v4** network filesystem
- [ ] **FUSE** (Filesystem in Userspace)

#### 4.4 Block Layer

- [ ] Block I/O request queue (BIO layer)
- [ ] ATA / AHCI (SATA) driver
- [ ] NVMe (PCIe SSD) driver
- [ ] `virtio-blk` support (QEMU)
- [ ] SCSI middle layer
- [ ] I/O schedulers: `none`, `mq-deadline`, `BFQ`

#### 4.5 LVM and RAID

- [ ] `device-mapper` support
- [ ] LVM (Logical Volume Manager) user tools
- [ ] Software RAID (`md`): RAID 0, 1, 5, 6
- [ ] `dm-crypt` block device encryption

---

### Phase 5 - Networking and IPC

**Timeline:** Months 12-18  
**Status:** 🔲 Not started  
**Dependencies:** Phase 1.8, Phase 4.1

#### 5.1 Network Stack (TCP/IP)

- [ ] Ethernet / L2 (MAC, ARP)
- [ ] IPv4: routing, fragmentation, TTL
- [ ] IPv6: SLAAC, DHCPv6
- [ ] TCP: handshake, sliding window, retransmit, Nagle
- [ ] UDP
- [ ] ICMP / ICMPv6
- [ ] Berkeley sockets API (`socket()`, `bind()`, `connect()`, `accept()`, ...)
- [ ] `select()`, `poll()`, `epoll()`
- [ ] Network namespaces (`netns`)

#### 5.2 Network Drivers

- [ ] `virtio-net` (QEMU/KVM)
- [ ] `e1000` / `e1000e` (Intel Gigabit Ethernet)
- [ ] `rtl8139` / `r8169` (Realtek)
- [ ] Loopback (`lo`)

#### 5.3 Networking Utilities

- [ ] `ip` (iproute2)
- [ ] `ping`, `traceroute`
- [ ] `netstat` / `ss`
- [ ] `dhcpcd` DHCP client
- [ ] `nftables` firewall
- [ ] `openssh` (`sshd` + `ssh` client)
- [ ] `curl` / `wget`

#### 5.4 IPC Mechanisms

- [ ] Pipes (anonymous and FIFO)
- [ ] UNIX domain sockets
- [ ] POSIX message queues (`mq_open`, `mq_send`, `mq_receive`)
- [ ] POSIX shared memory (`shm_open`, `mmap`)
- [ ] POSIX semaphores (`sem_open`, `sem_wait`)
- [ ] D-Bus (optional)

---

### Phase 6 - User Environment

**Timeline:** Months 16-22  
**Status:** 🔲 Not started  
**Dependencies:** Phase 3, Phase 4, Phase 5

#### 6.1 Init System

- [ ] Initial RAM disk (`initramfs` / `initrd`)
- [ ] Init system (GNU Shepherd-compatible)
- [ ] Service management: dependencies, restart, logging
- [ ] Runlevels / targets
- [ ] `udev` / `mdev` hotplug
- [ ] `dbus-daemon` system bus

#### 6.2 Users and Permissions

- [ ] `/etc/passwd`, `/etc/group`, `/etc/shadow`
- [ ] PAM (Pluggable Authentication Modules)
- [ ] `login`, `su`, `sudo`
- [ ] POSIX.1e capabilities (`cap_net_bind_service`, ...)
- [ ] User namespaces
- [ ] SELinux / AppArmor (MAC)

#### 6.3 Terminal Subsystem

- [ ] Pseudoterminals (PTY: `/dev/ptmx`, `/dev/pts/*`)
- [ ] TTY line discipline
- [ ] Virtual consoles (VT switching, Ctrl+Alt+F1..F6)
- [ ] Console terminal emulator

#### 6.4 System Services

- [ ] `syslogd` / `rsyslog`
- [ ] `crond`
- [ ] `ntpd` / `chrony`
- [ ] `nscd`
- [ ] `at` / `batch`

#### 6.5 Init and Configuration Scripts

- [ ] `/etc/profile`, `/etc/bashrc`
- [ ] `/etc/fstab`
- [ ] `/etc/hostname`, `/etc/hosts`, `/etc/resolv.conf`
- [ ] `/etc/os-release`
- [ ] Locale and i18n (`gettext`, `LC_*`, `/etc/locale.conf`)

---

### Phase 7 - Package Manager and Ecosystem

**Timeline:** Months 20-26  
**Status:** 🔲 Not started  
**Dependencies:** Phase 6

#### 7.1 Package Manager

- [ ] Package format (`tar.xz` + TOML/JSON metadata)
- [ ] Dependency resolution (SAT solver or topological sort)
- [ ] Package verification (GPG signatures, SHA-256)
- [ ] Atomic install / remove / rollback
- [ ] Package repository (HTTP/HTTPS mirrors)
- [ ] CLI tool: `gnupkg install`, `gnupkg update`, `gnupkg search`
- [ ] Package build system (ports)

#### 7.2 Core Package Set

- [ ] System: `openssl`, `zlib`, `xz`, `bzip2`, `lz4`, `zstd`
- [ ] Development: `python3`, `perl`, `ruby`
- [ ] Networking: `curl`, `wget`, `openssh`, `rsync`
- [ ] Editors: `nano`, `vim`, `emacs`
- [ ] Databases: `SQLite`
- [ ] Archivers: `tar`, `gzip`, `zip`, `unzip`, `p7zip`

#### 7.3 SDK and Developer Experience

- [ ] Sysroot package for cross-compilation
- [ ] `gnuos-devel` meta-package (headers, libs, compiler)
- [ ] Documentation in `info`/`man` formats
- [ ] Program examples and templates

---

### Phase 8 - Graphics Subsystem

**Timeline:** Months 24-32  
**Status:** 🔲 Not started  
**Dependencies:** Phase 6

#### 8.1 Low-Level Graphics Stack

- [ ] DRM / KMS
- [ ] `virtio-gpu` driver (QEMU)
- [ ] Basic Intel `i915` / AMD `amdgpu` drivers
- [ ] GBM (Generic Buffer Management)
- [ ] Mesa (OpenGL / Vulkan over DRM)

#### 8.2 Display Server

- [ ] Wayland compositor (Weston or custom)
- [ ] XWayland compatibility
- [ ] `libinput` integration

#### 8.3 GUI Toolkit Stack

- [ ] GTK 4
- [ ] Fonts: FreeType2, Fontconfig, HarfBuzz
- [ ] Cairo / Pango
- [ ] `hicolor` icon theme

#### 8.4 Base Graphical Applications

- [ ] Terminal emulator (GNOME Terminal / foot)
- [ ] File manager (Nautilus / Thunar)
- [ ] Text editor (gedit / Kate)
- [ ] Image viewer
- [ ] Web browser (Firefox or Epiphany/GNOME Web)

---

### Phase 9 - Security and Auditing

**Timeline:** Months 26-34 (parallel with Phase 8)  
**Status:** 🔲 Not started  
**Dependencies:** All previous phases

#### 9.1 Kernel Hardening

- [ ] KASLR (Kernel Address Space Layout Randomization)
- [ ] SMEP / SMAP
- [ ] Stack canaries (SSP) in all kernel modules
- [ ] CFI (Control Flow Integrity) for the kernel
- [ ] Seccomp BPF syscall filtering
- [ ] Syscall auditing (`auditd`)

#### 9.2 User-Space Hardening

- [ ] ASLR + PIE for all binaries
- [ ] RELRO
- [ ] `_FORTIFY_SOURCE=2`
- [ ] `-fstack-protector-strong`
- [ ] Hardened allocator (jemalloc with protections)

#### 9.3 Cryptography

- [ ] TLS 1.3 support (OpenSSL / GnuTLS)
- [ ] Certificate/key store (`/etc/ssl/certs`, NSS)
- [ ] `dm-crypt` + LUKS for partition encryption
- [ ] GPG for package signatures
- [ ] Hardware RNG (`/dev/hwrng`, TPM 2.0)

#### 9.4 Audit and Compliance

- [ ] POSIX test suite pass
- [ ] Fuzzing of all parsers (kernel + user-space)
- [ ] Static analysis over full codebase
- [ ] Vulnerability disclosure policy (CVE, `security@gnuos`)
- [ ] Security advisory/update process

---

### Phase 10 - Stabilization and Release

**Timeline:** Months 32-40  
**Status:** 🔲 Not started  
**Dependencies:** All previous phases

#### 10.1 Testing

- [ ] Full-stack integration tests (automated in QEMU)
- [ ] Real hardware tests (`x86_64`, `aarch64`)
- [ ] Long-running stress tests
- [ ] Regression tests (adapted LTP)
- [ ] Performance testing (`sysbench`, `iperf3`, `fio`)

#### 10.2 Documentation

- [ ] User guide (GNU manual style)
- [ ] System administrator guide
- [ ] Developer guide (kernel internals)
- [ ] Man pages for commands and syscalls
- [ ] Release notes

#### 10.3 First Stable Release (`v1.0`)

- [ ] Feature freeze
- [ ] Public beta testing
- [ ] Release Candidate cycle (RC1 -> RC2 -> RC3)
- [ ] Signed ISO images (GPG)
- [ ] System installer (text-mode + GUI)
- [ ] Live ISO boot mode
- [ ] Official announcement (FSF / GNU channels)

#### 10.4 Long-Term Support (LTS)

- [ ] LTS branch: 5 years of support after release
- [ ] Security patch backport policy
- [ ] Stable ABI for the 1.x series
- [ ] Regular point releases (1.0.1, 1.0.2, ...)

---

## Technology Stack

| Layer | Technology | Version / Note |
|---|---|---|
| Bootloader | GNU GRUB 2 | Multiboot2 + UEFI |
| Kernel language | C17 + asm | GNU C extensions |
| Compiler | GCC | >= 13.x |
| Linker | GNU ld / lld | ELF64 |
| C runtime | glibc | >= 2.38 |
| Shell | GNU Bash | >= 5.x |
| Build system | GNU Make + Autotools | |
| Emulator | QEMU | >= 8.x |
| VCS | Git | Monorepo |
| CI/CD | GitHub Actions | |
| Debugger | GDB | Remote kernel stub |
| Graphics | Mesa + Wayland | OpenGL 4.6 / Vulkan 1.3 |
| Crypto | OpenSSL / GnuTLS | >= 3.x |

---

## Repository Structure

```text
gnuos/
├── boot/                   # Bootloader and early init
│   ├── grub/               # GRUB config
│   └── efi/                # UEFI stub
├── kernel/                 # Kernel sources
│   ├── arch/               # Architecture-specific code
│   │   ├── x86_64/
│   │   ├── aarch64/
│   │   └── riscv64/
│   ├── mm/                 # Memory management
│   ├── sched/              # Scheduler
│   ├── ipc/                # Inter-process communication
│   ├── fs/                 # VFS and filesystems
│   ├── net/                # Network stack
│   ├── drivers/            # Device drivers
│   │   ├── block/
│   │   ├── char/
│   │   ├── net/
│   │   └── gpu/
│   └── security/           # LSM, capabilities, seccomp
├── lib/                    # Kernel libraries (libk)
├── userspace/              # User space
│   ├── libc/               # glibc port
│   ├── init/               # Init system
│   ├── coreutils/          # GNU Coreutils port
│   ├── bash/               # GNU Bash port
│   └── drivers/            # User-space drivers
├── pkg/                    # Package manager
│   ├── manager/            # gnupkg CLI
│   └── recipes/            # Package recipes
├── scripts/                # Build and automation scripts
│   ├── toolchain/          # Cross-toolchain scripts
│   ├── qemu/               # QEMU launch scripts
│   └── ci/                 # CI scripts
├── tests/                  # Tests
│   ├── unit/
│   ├── integration/
│   └── posix/              # POSIX conformance tests
├── docs/                   # Documentation
│   ├── adr/                # Architecture Decision Records
│   ├── kernel/             # Kernel docs
│   └── user/               # User documentation
├── include/                # Public headers
├── Makefile
├── configure
├── CONTRIBUTING.md
├── LICENSE
└── docs/ROADMAP.md
```

---

## Development Standards

### Coding

- Code style: **GNU Coding Standards** + project `clang-format`
- Public interfaces are documented in headers
- Commit messages follow **Conventional Commits**
- New behavior should include tests whenever possible
- `goto` is allowed only for structured error cleanup in kernel code
- Maximum line length: 100

### Naming

```c
/* Kernel functions: module prefix + verb_noun */
mm_alloc_page()
sched_enqueue_task()
vfs_mount_filesystem()

/* Structures: descriptive name + _t suffix */
typedef struct process process_t;
typedef struct inode inode_t;

/* Constants: MODULE_CONSTANT_NAME */
#define MM_PAGE_SIZE     4096
#define SCHED_MAX_PRIO   139
```

### Review Process

```text
feature branch -> PR -> CI passes -> 2x code review -> merge to main
                                          |
                                   security review for kernel changes
```

---

## Build System

### Quick Start

```bash
git clone https://github.com/meavxxxx/GnuOS.git
cd gnuos

# Toolchain/environment setup (or use Docker)
./scripts/toolchain/build-toolchain.sh

make ARCH=x86_64 kernel
make ARCH=x86_64 image
make ARCH=x86_64 run
```

### Docker Environment

```bash
docker build -t gnuos-dev ./scripts/docker/
docker run -it --rm -v $(pwd):/src gnuos-dev make ARCH=x86_64 all
```

### Make Targets

| Command | Description |
|---|---|
| `make kernel` | Build kernel |
| `make userspace` | Build userspace |
| `make image` | Build bootable image |
| `make iso` | Build ISO image |
| `make run` | Run in QEMU |
| `make run-debug` | Run with GDB stub |
| `make test` | Run tests |
| `make check-posix` | Run POSIX conformance tests |
| `make docs` | Generate documentation |
| `make clean` | Clean artifacts |

---

## Contributing

### Getting Started

1. Read `CONTRIBUTING.md`
2. Pick a `good-first-issue`
3. Discuss implementation in issue/discussion
4. Create a branch: `git checkout -b feat/your-feature`
5. Implement + test
6. Open a pull request

---

## Licensing

| Component | License |
|---|---|
| Kernel | **GPLv3** |
| System libraries (glibc) | **LGPLv2.1+** |
| GNU tools (bash, coreutils) | **GPLv3+** |
| Header files | **GPLv3+ with exception** |
| Documentation | **GFDLv1.3+** |

All contributions are accepted under the license of the relevant component.  
No Contributor License Agreement (CLA) is required.

---

## Glossary

| Term | Meaning |
|---|---|
| ABI | Application Binary Interface |
| ACPI | Advanced Configuration and Power Interface |
| ADR | Architecture Decision Record |
| ASLR | Address Space Layout Randomization |
| BIO | Block I/O |
| CoW | Copy-on-Write |
| DRM | Direct Rendering Manager |
| ELF | Executable and Linkable Format |
| GDT | Global Descriptor Table |
| IDT | Interrupt Descriptor Table |
| IPC | Inter-Process Communication |
| KASLR | Kernel Address Space Layout Randomization |
| KMS | Kernel Mode Setting |
| LTS | Long-Term Support |
| MAC | Mandatory Access Control |
| NUMA | Non-Uniform Memory Access |
| PCB | Process Control Block |
| PIE | Position Independent Executable |
| POSIX | Portable Operating System Interface |
| PTY | Pseudo-Terminal |
| RFC | Request for Comments |
| RELRO | Read-Only after Relocation |
| SMP | Symmetric Multiprocessing |
| TCB | Task Control Block |
| TLS | Thread-Local Storage |
| VFS | Virtual File System |
| VMM | Virtual Memory Manager |
