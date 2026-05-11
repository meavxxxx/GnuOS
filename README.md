# 🐂 GNU OS — Roadmap разработки операционной системы

> Полный план создания свободной операционной системы на базе GNU с нуля.  
> Следует принципам FSF, GPLv3 и философии свободного ПО Ричарда Столлмана.

---

## 📋 Содержание

- [Концепция проекта](#концепция-проекта)
- [Архитектура системы](#архитектура-системы)
- [Фазы разработки](#фазы-разработки)
  - [Фаза 0 — Подготовка и инфраструктура](#фаза-0--подготовка-и-инфраструктура)
  - [Фаза 1 — Ядро и загрузчик](#фаза-1--ядро-и-загрузчик)
  - [Фаза 2 — Системные библиотеки и ABI](#фаза-2--системные-библиотеки-и-abi)
  - [Фаза 3 — Инструментарий GNU](#фаза-3--инструментарий-gnu)
  - [Фаза 4 — Файловые системы и хранилище](#фаза-4--файловые-системы-и-хранилище)
  - [Фаза 5 — Сеть и IPC](#фаза-5--сеть-и-ipc)
  - [Фаза 6 — Пользовательское окружение](#фаза-6--пользовательское-окружение)
  - [Фаза 7 — Пакетный менеджер и экосистема](#фаза-7--пакетный-менеджер-и-экосистема)
  - [Фаза 8 — Графическая подсистема](#фаза-8--графическая-подсистема)
  - [Фаза 9 — Безопасность и аудит](#фаза-9--безопасность-и-аудит)
  - [Фаза 10 — Стабилизация и релиз](#фаза-10--стабилизация-и-релиз)
- [Технологический стек](#технологический-стек)
- [Структура репозитория](#структура-репозитория)
- [Стандарты разработки](#стандарты-разработки)
- [Сборочная система](#сборочная-система)
- [Участие в разработке](#участие-в-разработке)
- [Лицензирование](#лицензирование)
- [Глоссарий](#глоссарий)

---

## Концепция проекта

### Миссия

Создать полноценную свободную операционную систему, полностью совместимую со стандартом POSIX, реализующую собственное микроядро и полный пользовательский стек на базе GNU-инструментария.

### Принципы

| Принцип | Описание |
|---|---|
| **Свобода** | Весь код распространяется под GPLv3 или совместимыми лицензиями |
| **Прозрачность** | Открытая разработка, публичный трекер задач, публичные RFC |
| **Надёжность** | Стабильный ABI, формальная верификация критических модулей |
| **POSIX-совместимость** | Полная поддержка POSIX.1-2017 (IEEE Std 1003.1) |
| **Модульность** | Микроядерная архитектура — каждый компонент изолирован |

### Целевые платформы

- `x86_64` — основная целевая архитектура
- `aarch64` (ARM64) — вторичная поддержка
- `riscv64` — экспериментальная поддержка

---

## Архитектура системы

```
┌─────────────────────────────────────────────────────────────┐
│                    Приложения пользователя                  │
│         (bash, coreutils, editors, compilers, ...)          │
├─────────────────────────────────────────────────────────────┤
│                    GNU C Library (glibc)                    │
│              Системные вызовы / POSIX API                   │
├──────────────┬──────────────┬───────────────┬──────────────┤
│  VFS Layer   │  Net Stack   │  Device Layer │  IPC / RPC   │
├──────────────┴──────────────┴───────────────┴──────────────┤
│                     Серверы пространства пользователя       │
│        (файловые серверы, драйверы устройств, init)         │
├─────────────────────────────────────────────────────────────┤
│                     МИКРОЯДРО (Ring 0)                      │
│   Планировщик │ Управление памятью │ IPC │ Прерывания       │
├─────────────────────────────────────────────────────────────┤
│                   GNU GRUB / загрузчик                      │
├─────────────────────────────────────────────────────────────┤
│                        HARDWARE                             │
└─────────────────────────────────────────────────────────────┘
```

**Тип ядра:** Микроядро (вдохновлено GNU Hurd / L4)  
**Модель безопасности:** Capability-based security  
**ABI:** System V AMD64 ABI (x86_64), AAPCS64 (aarch64)

---

## Фазы разработки

---

### Фаза 0 — Подготовка и инфраструктура

**Срок:** Месяцы 1–2  
**Статус:** 🟡 В процессе

#### 0.1 Организационная инфраструктура

- [ ] Создание Git-монорепозитория с политиками ветвления
- [x] Настройка CI/CD пайплайна (builds, unit-тесты, интеграционные тесты)
- [ ] Публичный трекер задач (Savannah / GitLab)
- [ ] Система RFC (Request for Comments) для архитектурных решений
- [x] Кодекс участника (Code of Conduct)
- [x] Документация для контрибьюторов (`CONTRIBUTING.md`)
- [ ] Настройка mailing list (GNU Mailman)

#### 0.2 Кросс-компилятор и сборочная среда

- [x] Сборка кросс-компилятора `binutils` для целевых архитектур
- [x] Сборка кросс-компилятора `GCC` (C, C++) под `x86_64-gnuos-elf`
- [x] Настройка `QEMU` для эмуляции всех целевых платформ
- [x] Образ Docker / контейнер для воспроизводимой сборки
- [x] Скрипты автоматической настройки окружения разработчика

#### 0.3 Инструменты качества

- [ ] Статический анализатор (cppcheck, clang-tidy)
- [x] Форматтер кода (`clang-format`, `.editorconfig`)
- [ ] Покрытие кода (gcov / lcov)
- [ ] Fuzzing-инфраструктура (AFL++, libFuzzer)
- [x] Политика review: минимум 2 approve перед merge

#### 0.4 Документация

- [ ] Архитектурный документ (Architecture Decision Records — ADR)
- [x] Стандарты кодирования (GNU Coding Standards + расширения)
- [x] Шаблоны issues и pull requests
- [ ] Wiki с глоссарием и FAQ

---

### Фаза 1 — Ядро и загрузчик

**Срок:** Месяцы 3–8  
**Статус:** 🟡 В процессе  
**Зависимости:** Фаза 0

#### 1.1 Загрузчик (Bootloader)

- [x] Поддержка Multiboot2 протокола (совместимость с GRUB 2)
- [x] Настройка GDT (Global Descriptor Table) для x86_64
- [x] Переход в 64-битный защищённый режим (Long Mode)
- [x] Базовая инициализация стека
- [x] Передача управления ядру с картой памяти
- [ ] Поддержка UEFI (UEFI stub loader)
- [ ] Настройка framebuffer для раннего вывода (VESA / GOP)

#### 1.2 Начальная инициализация ядра

- [x] Раннее управление памятью (физическим аллокатором страниц)
- [x] Настройка IDT (Interrupt Descriptor Table)
- [x] Обработчики исключений CPU (page fault, GPF, #DE, ...)
- [ ] Настройка APIC / xAPIC / x2APIC
- [x] Базовый вывод через VGA/serial для отладки (`kprintf`)
- [ ] Инициализация ACPI (парсинг таблиц RSDP, MADT, FADT)
- [ ] Инициализация HPET / APIC Timer

#### 1.3 Управление физической памятью

- [ ] Физический аллокатор страниц (buddy allocator)
- [x] Парсинг E820 / UEFI Memory Map
- [ ] Поддержка NUMA (Non-Uniform Memory Access)
- [ ] Резервирование памяти для DMA, MMIO, ACPI
- [ ] Статистика использования памяти (`/proc/meminfo`-совместимая)

#### 1.4 Виртуальная память (VMM)

- [x] 4-уровневые таблицы страниц (PML4) для x86_64
- [ ] Kernel virtual address space layout (KASLR-ready)
- [x] Аллокатор виртуального адресного пространства
- [x] Поддержка huge pages (2 МБ, 1 ГБ)
- [ ] Copy-on-Write (CoW) для fork()
- [ ] Demand paging (отложенная загрузка страниц)
- [ ] mmap() / munmap() / mprotect()
- [ ] Поддержка SMEP / SMAP / NX bit

#### 1.5 Планировщик процессов

- [x] Базовые структуры: Task Control Block (TCB), Process Control Block (PCB)
- [ ] Вытесняющая многозадачность (preemptive multitasking)
- [ ] Алгоритм планирования: CFS (Completely Fair Scheduler)
- [ ] Поддержка приоритетов и классов планирования (SCHED_FIFO, SCHED_RR, SCHED_OTHER)
- [ ] SMP-поддержка: балансировка нагрузки между CPU
- [ ] Контексты ядра и пользователя: сохранение/восстановление регистров
- [ ] Таймер планировщика (tick-based → tickless)
- [ ] Idle task на каждое ядро
- [ ] /proc/[pid]/stat совместимая статистика

#### 1.6 Управление процессами

- [ ] fork() / vfork() / clone()
- [ ] exec() семейство (execve, execvp, ...)
- [ ] wait() / waitpid() / waitid()
- [ ] exit() / _exit() с корректной очисткой ресурсов
- [ ] Сигналы POSIX (kill, sigaction, sigprocmask, ...)
- [ ] Группы процессов, сессии, управляющий терминал
- [ ] Пространства имён (namespaces): PID, mount, UTS, IPC, net

#### 1.7 Потоки ядра и синхронизация

- [x] Потоки ядра (kernel threads)
- [x] Мьютексы, спинлоки, RW-локи
- [x] Атомарные операции (на базе `__atomic_*`)
- [x] Очереди ожидания (wait queues)
- [x] Механизм work queues для отложенных задач
- [x] Поддержка RCU (Read-Copy-Update)

#### 1.8 Микроядерный IPC

- [x] Синхронная передача сообщений (rendezvous)
- [x] Асинхронные очереди сообщений
- [x] Передача capability (файловые дескрипторы, права доступа)
- [x] Разделяемая память между процессами (SHM)
- [x] Именованные каналы ядра

#### 1.9 Драйверы низкого уровня

- [x] Последовательный порт (UART 16550) — отладочный вывод
- [x] PS/2 клавиатура (базовая)
- [x] Буфер ввода клавиатуры и преобразование scancode -> ASCII
- [x] PCI / PCIe сканирование и перечисление устройств
- [x] MSI / MSI-X прерывания
- [x] DMA engine (базовый)

---

### Фаза 2 — Системные библиотеки и ABI

**Срок:** Месяцы 7–12  
**Статус:** 🔲 Не начато  
**Зависимости:** Фаза 1.1–1.6

#### 2.1 Слой системных вызовов

- [x] Таблица syscall (syscall_table) с номерами, совместимыми с Linux ABI (опционально)
- [x] Вход/выход через `syscall`/`sysret` (x86_64 fast path)
- [x] Валидация пользовательских указателей перед разыменованием
- [ ] Аудит системных вызовов (seccomp-совместимый фильтр)
- [ ] Тесты для каждого syscall

#### 2.2 GNU C Library (glibc) — портирование

- [ ] Портирование glibc на новую платформу (`gnuos` target)
- [ ] Реализация `start files` (crt0.S, crti.S, crtn.S)
- [ ] Поддержка динамической линковки (ld.so / PT_INTERP)
- [ ] POSIX threads (pthreads) поверх clone()
- [ ] Поддержка TLS (Thread-Local Storage, `%fs` base)
- [ ] Реализация `dl_iterate_phdr`, `backtrace()`

#### 2.3 Динамический компоновщик

- [ ] ELF загрузчик (парсинг PT_LOAD, PT_DYNAMIC, PT_GNU_RELRO)
- [ ] Разрешение символов (PLT / GOT)
- [ ] Порядок инициализации: `.init_array`, `DT_INIT`
- [ ] `dlopen()` / `dlsym()` / `dlclose()`
- [ ] LD_PRELOAD механизм
- [ ] ASLR для shared libraries

#### 2.4 Системная математическая библиотека

- [ ] Портирование libm (GNU libm или CORE-MATH)
- [ ] Поддержка IEEE 754 (round modes, NaN, Inf)
- [ ] Аппаратное ускорение (SSE2, AVX для x86_64)

#### 2.5 Стандартные заголовки и POSIX-совместимость

- [ ] Полный набор `<unistd.h>`, `<sys/types.h>`, `<sys/stat.h>`, ...
- [ ] `<signal.h>` с полным набором сигналов POSIX
- [ ] `<pthread.h>` и расширения (`pthread_attr_*`, `sem_*`, ...)
- [ ] `<sys/socket.h>`, `<netinet/in.h>`, `<arpa/inet.h>`
- [ ] Тест-сьют POSIX Conformance (POSIX Test Suite / LTP)

---

### Фаза 3 — Инструментарий GNU

**Срок:** Месяцы 10–16  
**Статус:** 🔲 Не начато  
**Зависимости:** Фаза 2

#### 3.1 Портирование GNU Coreutils

- [ ] Базовые утилиты: `ls`, `cp`, `mv`, `rm`, `mkdir`, `rmdir`
- [ ] Текстовые утилиты: `cat`, `echo`, `printf`, `tee`, `head`, `tail`
- [ ] Сортировка и фильтрация: `sort`, `uniq`, `wc`, `grep`, `sed`, `awk`
- [ ] Утилиты прав доступа: `chmod`, `chown`, `chgrp`, `umask`
- [ ] Системная информация: `uname`, `hostname`, `uptime`, `df`, `du`
- [ ] Управление процессами: `ps`, `kill`, `nice`, `nohup`

#### 3.2 GNU Bash

- [ ] Портирование GNU Bash (последняя стабильная версия)
- [ ] Интерактивный режим (readline, history, tab completion)
- [ ] Скриптовый режим (полная POSIX sh совместимость)
- [ ] Job control (fg, bg, jobs, wait)
- [ ] Переменные окружения и экспорт

#### 3.3 GNU Binutils

- [ ] `as` — GNU Assembler для x86_64, aarch64
- [ ] `ld` — GNU Linker с поддержкой ELF, скриптов линковки
- [ ] `objdump`, `readelf`, `nm`, `strings`, `strip`
- [ ] `ar`, `ranlib` — статические библиотеки
- [ ] `addr2line` — декодирование адресов в символы

#### 3.4 GCC — GNU Compiler Collection

- [ ] Портирование GCC на `x86_64-gnuos` target
- [ ] Поддержка C11, C17, C23
- [ ] Поддержка C++17, C++20
- [ ] LTO (Link-Time Optimization)
- [ ] Санитайзеры: AddressSanitizer, UBSan, ThreadSanitizer
- [ ] Профилирование: gprof, gcov

#### 3.5 GNU Make и системы сборки

- [ ] Портирование GNU Make
- [ ] Портирование Autoconf / Automake / Libtool
- [ ] Поддержка pkg-config
- [ ] Cmake (опционально)

#### 3.6 Текстовые редакторы и утилиты

- [ ] GNU nano (лёгкий редактор)
- [ ] GNU Emacs (полнофункциональный редактор)
- [ ] less, more
- [ ] diff, patch

---

### Фаза 4 — Файловые системы и хранилище

**Срок:** Месяцы 9–15  
**Статус:** 🔲 Не начато  
**Зависимости:** Фаза 1.4

#### 4.1 VFS (Virtual File System)

- [ ] Абстракция VFS: inode, dentry, file, superblock
- [ ] Кэш inode и dentry
- [ ] Page cache (страничный кэш, интеграция с VMM)
- [ ] Unified buffer cache
- [ ] Поддержка mount / umount / pivot_root
- [ ] Bind mounts, overlay mounts
- [ ] `/proc`, `/sys`, `/dev` — псевдофайловые системы

#### 4.2 Основные файловые системы

- [ ] **ext4** — основная FS: полная поддержка чтения/записи, journaling, extents
- [ ] **tmpfs** — временная FS в памяти (для `/tmp`, `/run`)
- [ ] **devtmpfs** — автоматическое создание устройств
- [ ] **procfs** (`/proc`) — информация о процессах
- [ ] **sysfs** (`/sys`) — дерево устройств ядра
- [ ] **FAT32 / exFAT** — совместимость со съёмными носителями

#### 4.3 Дополнительные файловые системы (опционально)

- [ ] **Btrfs** — Copy-on-Write, снапшоты, RAID
- [ ] **XFS** — высокопроизводительная FS
- [ ] **ISO 9660 / UDF** — поддержка оптических носителей
- [ ] **NFS v4** — сетевая файловая система
- [ ] **FUSE** (Filesystem in Userspace)

#### 4.4 Блочный уровень

- [ ] Очередь запросов блочных устройств (BIO layer)
- [ ] Драйвер ATA / AHCI (SATA)
- [ ] Драйвер NVMe (PCIe SSD)
- [ ] Поддержка virtio-blk (для QEMU)
- [ ] SCSI middle layer
- [ ] Планировщики I/O: none, mq-deadline, BFQ

#### 4.5 LVM и RAID

- [ ] Поддержка device-mapper
- [ ] LVM (Logical Volume Manager) — userspace tools
- [ ] Software RAID (md устройства): RAID 0, 1, 5, 6
- [ ] dm-crypt (шифрование блочных устройств)

---

### Фаза 5 — Сеть и IPC

**Срок:** Месяцы 12–18  
**Статус:** 🔲 Не начато  
**Зависимости:** Фаза 1.8, Фаза 4.1

#### 5.1 Сетевой стек (TCP/IP)

- [ ] Ethernet / L2 (MAC, ARP)
- [ ] IPv4: маршрутизация, фрагментация, TTL
- [ ] IPv6: автоконфигурация (SLAAC), DHCPv6
- [ ] TCP: three-way handshake, скользящее окно, retransmit, Nagle
- [ ] UDP
- [ ] ICMP / ICMPv6
- [ ] Berkeley Sockets API (`socket()`, `bind()`, `connect()`, `accept()`, ...)
- [ ] `select()`, `poll()`, `epoll()`
- [ ] Сетевые пространства имён (netns)

#### 5.2 Сетевые драйверы

- [ ] virtio-net (QEMU/KVM)
- [ ] e1000 / e1000e (Intel Gigabit Ethernet)
- [ ] rtl8139 / r8169 (Realtek)
- [ ] Loopback (`lo`)

#### 5.3 Сетевые утилиты

- [ ] `ip` (iproute2) — управление интерфейсами и маршрутами
- [ ] `ping`, `traceroute`
- [ ] `netstat` / `ss`
- [ ] `dhcpcd` — DHCP клиент
- [ ] `nftables` — пакетный фильтр (firewall)
- [ ] `openssh` (sshd + ssh клиент)
- [ ] `curl` / `wget`

#### 5.4 IPC механизмы

- [ ] Pipes (anonymous, named — FIFO)
- [ ] UNIX domain sockets
- [ ] POSIX message queues (`mq_open`, `mq_send`, `mq_receive`)
- [ ] POSIX shared memory (`shm_open`, `mmap`)
- [ ] Semaphores (POSIX: `sem_open`, `sem_wait`)
- [ ] D-Bus (опционально, для сервисов)

---

### Фаза 6 — Пользовательское окружение

**Срок:** Месяцы 16–22  
**Статус:** 🔲 Не начато  
**Зависимости:** Фаза 3, Фаза 4, Фаза 5

#### 6.1 Система инициализации

- [ ] Начальный RAM-диск (initramfs / initrd)
- [ ] Система инициализации (GNU Shepherd — совместимая с системным управлением)
- [ ] Service management: зависимости, перезапуск, логирование
- [ ] Управление runlevel / targets
- [ ] udev / mdev — горячее подключение устройств
- [ ] `dbus-daemon` — системная шина сообщений

#### 6.2 Пользователи и права доступа

- [ ] `/etc/passwd`, `/etc/group`, `/etc/shadow`
- [ ] PAM (Pluggable Authentication Modules)
- [ ] `login`, `su`, `sudo`
- [ ] Capabilities (POSIX.1e capabilities: `cap_net_bind_service`, ...)
- [ ] Пространства имён пользователей (user namespaces)
- [ ] SELinux / AppArmor (MAC — Mandatory Access Control)

#### 6.3 Терминальная подсистема

- [ ] Псевдотерминалы (PTY: `/dev/ptmx`, `/dev/pts/*`)
- [ ] TTY line discipline
- [ ] Virtual consoles (VT switching, Ctrl+Alt+F1..F6)
- [ ] Terminal emulator (консольный: для текстового режима)

#### 6.4 Системные сервисы

- [ ] `syslogd` / `rsyslog` — системный журнал
- [ ] `crond` — планировщик задач (crontab)
- [ ] `ntpd` / `chrony` — синхронизация времени
- [ ] `nscd` — кэш системных баз данных
- [ ] `at` / `batch` — одноразовые задачи

#### 6.5 Скрипты инициализации и конфигурации

- [ ] `/etc/profile`, `/etc/bashrc`
- [ ] `/etc/fstab` — монтирование файловых систем
- [ ] `/etc/hostname`, `/etc/hosts`, `/etc/resolv.conf`
- [ ] `/etc/os-release` (стандарт freedesktop)
- [ ] locale и internationalization (gettext, LC_*, `/etc/locale.conf`)

---

### Фаза 7 — Пакетный менеджер и экосистема

**Срок:** Месяцы 20–26  
**Статус:** 🔲 Не начато  
**Зависимости:** Фаза 6

#### 7.1 Пакетный менеджер

- [ ] Формат пакета (tar.xz + метаданные в TOML/JSON)
- [ ] Разрешение зависимостей (SAT-solver или топологическая сортировка)
- [ ] Верификация пакетов (GPG подпись, хэши SHA-256)
- [ ] Атомарные установка / удаление / откат
- [ ] Репозиторий пакетов (HTTP/HTTPS зеркало)
- [ ] CLI инструмент: `gnupkg install`, `gnupkg update`, `gnupkg search`
- [ ] Build-система для создания пакетов (порт-система)

#### 7.2 Основной набор пакетов

- [ ] Системные: openssl, zlib, xz, bzip2, lz4, zstd
- [ ] Разработка: python3, perl, ruby
- [ ] Сеть: curl, wget, openssh, rsync
- [ ] Редакторы: nano, vim, emacs
- [ ] Базы данных: SQLite
- [ ] Архиваторы: tar, gzip, zip, unzip, p7zip

#### 7.3 SDK и среда разработки

- [ ] Sysroot пакет для кросс-компиляции
- [ ] `gnuos-devel` метапакет (заголовки, библиотеки, компилятор)
- [ ] Документация в формате info/man
- [ ] Примеры и шаблоны программ

---

### Фаза 8 — Графическая подсистема

**Срок:** Месяцы 24–32  
**Статус:** 🔲 Не начато  
**Зависимости:** Фаза 6

#### 8.1 Графический стек (низкий уровень)

- [ ] DRM / KMS (Direct Rendering Manager / Kernel Mode Setting)
- [ ] Драйвер virtio-gpu (для QEMU)
- [ ] Базовый драйвер для Intel i915 / AMD amdgpu
- [ ] GBM (Generic Buffer Management)
- [ ] Mesa (OpenGL / Vulkan поверх DRM)

#### 8.2 Display Server

- [ ] Wayland compositor (Weston или собственный)
- [ ] XWayland для совместимости с X11 приложениями
- [ ] libinput — обработка устройств ввода

#### 8.3 GUI-инструментарий

- [ ] GTK 4 — основной GUI toolkit
- [ ] Шрифты: FreeType2, Fontconfig, HarfBuzz (шейпинг)
- [ ] Cairo / Pango — 2D рендеринг и текст
- [ ] Icon theme (hicolor)

#### 8.4 Базовые графические приложения

- [ ] Эмулятор терминала (GNOME Terminal / foot)
- [ ] Файловый менеджер (Nautilus / Thunar)
- [ ] Текстовый редактор (gedit / Kate)
- [ ] Просмотрщик изображений
- [ ] Веб-браузер (Firefox или Epiphany/GNOME Web)

---

### Фаза 9 — Безопасность и аудит

**Срок:** Месяцы 26–34 (параллельно с 8)  
**Статус:** 🔲 Не начато  
**Зависимости:** Все предыдущие фазы

#### 9.1 Ядро

- [ ] KASLR (Kernel Address Space Layout Randomization)
- [ ] SMEP / SMAP — защита от атак из пользовательского пространства
- [ ] Stack Canaries (SSP) во всех модулях ядра
- [ ] CFI (Control Flow Integrity) для ядра
- [ ] Seccomp BPF — фильтрация системных вызовов
- [ ] Аудит системных вызовов (auditd)

#### 9.2 Пользовательское пространство

- [ ] ASLR + PIE для всех бинарей
- [ ] RELRO (Read-Only after Relocation)
- [ ] Fortify Source (`_FORTIFY_SOURCE=2`)
- [ ] Stack-protector-strong
- [ ] Hardened malloc (jemalloc с защитами)

#### 9.3 Криптография

- [ ] Встроенная поддержка TLS 1.3 (OpenSSL / GnuTLS)
- [ ] Хранилище ключей (/etc/ssl/certs, NSS)
- [ ] dm-crypt + LUKS для шифрования разделов
- [ ] GPG (GNU Privacy Guard) для подписей пакетов
- [ ] Аппаратный RNG (`/dev/hwrng`, TPM 2.0)

#### 9.4 Аудит и соответствие

- [ ] Прохождение POSIX Test Suite
- [ ] Fuzzing всех парсеров (ядро + userspace)
- [ ] Статический анализ всей кодовой базы
- [ ] Политика раскрытия уязвимостей (CVE, security@gnuos)
- [ ] Процедура обновления безопасности (Security Advisory)

---

### Фаза 10 — Стабилизация и релиз

**Срок:** Месяцы 32–40  
**Статус:** 🔲 Не начато  
**Зависимости:** Все предыдущие фазы

#### 10.1 Тестирование

- [ ] Интеграционные тесты всего стека (автоматические в QEMU)
- [ ] Тесты на реальном железе (x86_64, aarch64)
- [ ] Стресс-тестирование (многосуточный прогон)
- [ ] Регрессионные тесты (LTP — Linux Test Project адаптация)
- [ ] Тесты производительности (sysbench, iperf3, fio)

#### 10.2 Документация

- [ ] Руководство пользователя (GNU manual стиль)
- [ ] Руководство системного администратора
- [ ] Руководство разработчика (kernel internals)
- [ ] man-страницы для всех команд и системных вызовов
- [ ] Release notes

#### 10.3 Первый стабильный релиз (v1.0)

- [ ] Feature freeze — заморозка функционала
- [ ] Бета-тестирование (публичное)
- [ ] Release Candidate (RC) цикл (RC1 → RC2 → RC3)
- [ ] Подписанные образы ISO (GPG)
- [ ] Установщик системы (text-mode + GUI)
- [ ] Live ISO (загрузка без установки)
- [ ] Официальное объявление (FSF, GNU mailing lists)

#### 10.4 Долгосрочная поддержка (LTS)

- [ ] LTS ветка: поддержка 5 лет после релиза
- [ ] Политика backport security-патчей
- [ ] Стабильный ABI для 1.x серии
- [ ] Регулярный выпуск point releases (1.0.1, 1.0.2, ...)

---

## Технологический стек

| Уровень | Технология | Версия / Примечание |
|---|---|---|
| Загрузчик | GNU GRUB 2 | Multiboot2 + UEFI |
| Язык ядра | C17 + asm | GNU C extensions |
| Компилятор | GCC | ≥ 13.x |
| Линковщик | GNU ld / lld | ELF64 |
| C Runtime | glibc | ≥ 2.38 |
| Shell | GNU Bash | ≥ 5.x |
| Build System | GNU Make + Autotools | |
| Эмулятор | QEMU | ≥ 8.x |
| VCS | Git | Monorepo |
| CI/CD | GitLab CI / GitHub Actions | |
| Отладчик | GDB | Remote stub в ядре |
| Графика | Mesa + Wayland | OpenGL 4.6 / Vulkan 1.3 |
| Crypto | OpenSSL / GnuTLS | ≥ 3.x |

---

## Структура репозитория

```
gnuos/
├── boot/                   # Загрузчик и early init
│   ├── grub/               # Конфигурация GRUB
│   └── efi/                # UEFI stub
├── kernel/                 # Исходники ядра
│   ├── arch/               # Архитектурно-зависимый код
│   │   ├── x86_64/
│   │   ├── aarch64/
│   │   └── riscv64/
│   ├── mm/                 # Управление памятью
│   ├── sched/              # Планировщик
│   ├── ipc/                # Межпроцессное взаимодействие
│   ├── fs/                 # VFS и файловые системы
│   ├── net/                # Сетевой стек
│   ├── drivers/            # Драйверы устройств
│   │   ├── block/
│   │   ├── char/
│   │   ├── net/
│   │   └── gpu/
│   └── security/           # LSM, capabilities, seccomp
├── lib/                    # Библиотеки ядра (libk)
├── userspace/              # Пользовательское пространство
│   ├── libc/               # glibc порт
│   ├── init/               # Система инициализации
│   ├── coreutils/          # GNU Coreutils порт
│   ├── bash/               # GNU Bash порт
│   └── drivers/            # Userspace драйверы
├── pkg/                    # Пакетный менеджер
│   ├── manager/            # gnupkg инструмент
│   └── recipes/            # Рецепты сборки пакетов
├── scripts/                # Скрипты сборки и автоматизации
│   ├── toolchain/          # Сборка кросс-компилятора
│   ├── qemu/               # Запуск в эмуляторе
│   └── ci/                 # CI/CD скрипты
├── tests/                  # Тесты
│   ├── unit/
│   ├── integration/
│   └── posix/              # POSIX conformance tests
├── docs/                   # Документация
│   ├── adr/                # Architecture Decision Records
│   ├── kernel/             # Документация ядра
│   └── user/               # Пользовательская документация
├── include/                # Публичные заголовки
├── Makefile                # Главный Makefile
├── configure               # Autoconf configure
├── CONTRIBUTING.md
├── LICENSE                 # GPLv3
└── README.md               # Этот файл
```

---

## Стандарты разработки

### Кодирование

- Стиль кода: **GNU Coding Standards** + `clang-format` с конфигом проекта
- Все публичные функции документируются в заголовочных файлах
- Commit messages: **Conventional Commits** (`feat:`, `fix:`, `docs:`, ...)
- Каждый PR обязан содержать unit-тесты для нового функционала
- Запрещено использование `goto` кроме обработки ошибок в ядре
- Максимальная длина строки: 100 символов

### Именование

```c
/* Функции ядра: префикс модуля + глагол_существительное */
mm_alloc_page()
sched_enqueue_task()
vfs_mount_filesystem()

/* Структуры: описательное имя + суффикс _t */
typedef struct process process_t;
typedef struct inode inode_t;

/* Константы: МОДУЛЬ_ИМЯ_КОНСТАНТЫ */
#define MM_PAGE_SIZE     4096
#define SCHED_MAX_PRIO   139
```

### Review процесс

```
feature branch → PR → CI passes → 2x Code Review → merge to main
                                     ↓
                              security review (для изменений ядра)
```

---

## Сборочная система

### Быстрый старт

```bash
# Клонирование
git clone https://savannah.gnu.org/git/gnuos.git
cd gnuos

# Настройка окружения (или использовать Docker)
./scripts/toolchain/build-toolchain.sh

# Сборка ядра
make ARCH=x86_64 kernel

# Сборка полного образа
make ARCH=x86_64 image

# Запуск в QEMU
make ARCH=x86_64 run
```

### Docker окружение

```bash
# Сборка контейнера разработки
docker build -t gnuos-dev ./scripts/docker/

# Запуск с монтированием репозитория
docker run -it --rm -v $(pwd):/src gnuos-dev make ARCH=x86_64 all
```

### Целевые команды Make

| Команда | Описание |
|---|---|
| `make kernel` | Сборка ядра |
| `make userspace` | Сборка пользовательских программ |
| `make image` | Создание загрузочного образа |
| `make iso` | Создание ISO образа |
| `make run` | Запуск в QEMU |
| `make run-debug` | Запуск с GDB stub |
| `make test` | Запуск тестов |
| `make check-posix` | POSIX conformance тесты |
| `make docs` | Генерация документации |
| `make clean` | Очистка артефактов |

---

## Участие в разработке

### Как начать

1. Прочитать `CONTRIBUTING.md`
2. Выбрать задачу с меткой `good-first-issue` в трекере
3. Обсудить подход в mailing list или issue
4. Создать ветку: `git checkout -b feat/your-feature`
5. Написать код + тесты
6. Открыть Pull Request

### Области для вклада

| Область | Уровень сложности | Что нужно |
|---|---|---|
| Документация | 🟢 Начинающий | Grамотность, знание темы |
| Тесты | 🟢 Начинающий | C, знание POSIX |
| Coreutils / Shell | 🟡 Средний | C, POSIX |
| Файловые системы | 🔴 Продвинутый | C, ядерное программирование |
| Сеть | 🔴 Продвинутый | C, TCP/IP |
| Ядро / MMU | 🔴 Эксперт | C + asm, архитектура CPU |
| Безопасность | 🔴 Эксперт | C, CVE, крипто |

### Коммуникация

- **Mailing list:** `gnuos-dev@gnu.org`
- **IRC:** `#gnuos` на `irc.libera.chat`
- **Трекер задач:** `https://savannah.gnu.org/projects/gnuos`
- **Документация:** `https://gnuos.gnu.org/docs`

---

## Лицензирование

| Компонент | Лицензия |
|---|---|
| Ядро | **GPLv2-only** (как Linux) или **GPLv3** |
| Системные библиотеки (glibc) | **LGPLv2.1+** |
| GNU-инструменты (bash, coreutils) | **GPLv3+** |
| Заголовочные файлы | **GPLv3+ with exception** |
| Документация | **GFDLv1.3+** |

Все вклады в проект принимаются под соответствующей лицензией компонента.  
Contributor License Agreement (CLA) **не требуется** — вы сохраняете авторские права.

---

## Глоссарий

| Термин | Расшифровка |
|---|---|
| ABI | Application Binary Interface |
| ACPI | Advanced Configuration and Power Interface |
| ADR | Architecture Decision Record |
| ASLR | Address Space Layout Randomization |
| BIO | Block I/O (уровень блочного ввода-вывода) |
| CoW | Copy-on-Write |
| DRM | Direct Rendering Manager |
| ELF | Executable and Linkable Format |
| GDT | Global Descriptor Table |
| IDT | Interrupt Descriptor Table |
| IPC | Inter-Process Communication |
| KASLR | Kernel ASLR |
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
| SMP | Symmetric Multi-Processing |
| TCB | Task Control Block |
| TLS | Thread-Local Storage |
| VFS | Virtual File System |
| VMM | Virtual Memory Manager |

---

<div align="center">

**GNU OS** — свободная операционная система для свободных людей.  
*"Free as in freedom."* — Richard M. Stallman

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![POSIX Compliant](https://img.shields.io/badge/POSIX-1003.1--2017-green.svg)](https://pubs.opengroup.org/onlinepubs/9699919799/)
[![GNU Project](https://img.shields.io/badge/GNU-Project-red.svg)](https://www.gnu.org/)

</div>
