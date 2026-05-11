ARCH ?= x86_64

-include config.mk

ifeq ($(ARCH),x86_64)
TARGET ?= x86_64-elf
QEMU_SYSTEM ?= qemu-system-x86_64
KERNEL_LINKER_SCRIPT := kernel/arch/x86_64/linker.ld
ARCH_C_SOURCES := kernel/arch/x86_64/serial.c
ARCH_C_SOURCES += kernel/arch/x86_64/interrupts/pic.c
ARCH_C_SOURCES += kernel/arch/x86_64/timer/pit.c
ARCH_C_SOURCES += kernel/arch/x86_64/syscall/fastpath.c
ARCH_ASM_SOURCES := kernel/arch/x86_64/boot/multiboot2_header.S \
	kernel/arch/x86_64/boot/entry.S \
	kernel/arch/x86_64/syscall/entry.S \
	kernel/arch/x86_64/sched/context_switch.S
else
$(error Unsupported ARCH "$(ARCH)". Supported now: x86_64)
endif

CC ?= $(TARGET)-gcc
OBJCOPY ?= $(TARGET)-objcopy

CSTD ?= gnu17
BUILD_DIR ?= build/$(ARCH)
ISO_DIR := $(BUILD_DIR)/iso
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
ISO_IMAGE := $(BUILD_DIR)/gnuos-$(ARCH).iso
GNUOS_TARGET ?= x86_64-gnuos
USER_STARTFILES_DIR := userspace/libc/startfiles/$(ARCH)
USER_STARTFILES_SOURCES := \
	$(USER_STARTFILES_DIR)/crt0.S \
	$(USER_STARTFILES_DIR)/crti.S \
	$(USER_STARTFILES_DIR)/crtn.S
USER_STARTFILES_OBJECTS := \
	$(patsubst $(USER_STARTFILES_DIR)/%.S,$(BUILD_DIR)/$(USER_STARTFILES_DIR)/%.o,$(USER_STARTFILES_SOURCES))
USER_SMOKE_SOURCE := userspace/init/init_minimal.c
USER_SMOKE_OBJECT := $(BUILD_DIR)/$(USER_SMOKE_SOURCE:.c=.o)
USER_SMOKE_ELF := $(BUILD_DIR)/userspace/init/init_minimal.elf
USER_HEADERS_DIR := userspace/libc/include
USER_SYSROOT_DIR := $(BUILD_DIR)/sysroot/$(GNUOS_TARGET)
USER_CFLAGS := -std=$(CSTD) -O2 -g -ffreestanding -fno-stack-protector -fno-pie \
	-Wall -Wextra -Wpedantic --sysroot=$(USER_SYSROOT_DIR) -isystem $(USER_SYSROOT_DIR)/usr/include

COMMON_CFLAGS := -std=$(CSTD) -O2 -g -ffreestanding -fno-stack-protector -fno-pie \
	-mno-red-zone -mgeneral-regs-only -Wall -Wextra -Wpedantic -Iinclude -Ikernel/include
COMMON_LDFLAGS := -nostdlib -no-pie -Wl,--build-id=none,-n,-T,$(KERNEL_LINKER_SCRIPT)

KERNEL_C_SOURCES := \
	kernel/init/kmain.c \
	kernel/init/panic.c \
	kernel/arch/x86_64/interrupts/idt.c \
	kernel/drivers/char/ps2_keyboard.c \
	kernel/drivers/pci.c \
	kernel/drivers/dma.c \
	kernel/ipc/ipc.c \
	kernel/ipc/shm.c \
	kernel/mm/pmm.c \
	kernel/mm/uaccess.c \
	kernel/mm/vmm.c \
	kernel/sched/sched.c \
	kernel/sched/workqueue.c \
	kernel/lib/spinlock.c \
	kernel/lib/mutex.c \
	kernel/lib/rwlock.c \
	kernel/lib/rcu.c \
	kernel/lib/printk.c \
	kernel/security/capability.c \
	kernel/security/seccomp.c \
	kernel/syscall/syscall.c \
	kernel/arch/x86_64/boot/multiboot2.c

KERNEL_OBJECTS := \
	$(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_C_SOURCES) $(ARCH_C_SOURCES)) \
	$(patsubst %.S,$(BUILD_DIR)/%.o,$(ARCH_ASM_SOURCES))
KERNEL_DEPS := $(KERNEL_OBJECTS:.o=.d)

.PHONY: all kernel userspace userspace-startfiles userspace-sysroot userspace-smoke image iso run run-debug test check-posix docs clean

all: kernel

kernel: $(KERNEL_ELF)

$(KERNEL_ELF): $(KERNEL_OBJECTS) $(KERNEL_LINKER_SCRIPT)
	@mkdir -p $(dir $@)
	$(CC) $(COMMON_CFLAGS) $(COMMON_LDFLAGS) -o $@ $(KERNEL_OBJECTS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(COMMON_CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(COMMON_CFLAGS) -MMD -MP -c $< -o $@

image iso: $(ISO_IMAGE)

$(ISO_IMAGE): $(KERNEL_ELF) boot/grub/grub.cfg
	@mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL_ELF) $(ISO_DIR)/boot/kernel.elf
	cp boot/grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(ISO_DIR)

run: $(ISO_IMAGE)
	$(QEMU_SYSTEM) -cdrom $(ISO_IMAGE) -serial stdio

run-debug: $(ISO_IMAGE)
	$(QEMU_SYSTEM) -cdrom $(ISO_IMAGE) -serial stdio -s -S

userspace: userspace-smoke

userspace-startfiles: $(USER_STARTFILES_OBJECTS)
	@echo "GNU OS userspace start files built:"
	@for obj in $(USER_STARTFILES_OBJECTS); do echo "  $$obj"; done

userspace-sysroot: userspace-startfiles
	bash scripts/toolchain/install-gnuos-sysroot.sh \
		ARCH=$(ARCH) \
		TARGET=$(GNUOS_TARGET) \
		BUILD_DIR=$(BUILD_DIR) \
		SYSROOT=$(USER_SYSROOT_DIR) \
		HEADERS_DIR=$(USER_HEADERS_DIR) \
		STARTFILES_DIR=$(BUILD_DIR)/$(USER_STARTFILES_DIR)

userspace-smoke: userspace-sysroot $(USER_SMOKE_ELF)
	@echo "GNU OS userspace smoke ELF: $(USER_SMOKE_ELF)"

$(USER_SMOKE_ELF): $(USER_SMOKE_OBJECT) userspace-sysroot
	@mkdir -p $(dir $@)
	$(CC) -nostdlib -no-pie -Wl,--build-id=none -o $@ \
		$(USER_SYSROOT_DIR)/usr/lib/crt0.o \
		$(USER_SYSROOT_DIR)/usr/lib/crti.o \
		$(USER_SMOKE_OBJECT) \
		$(USER_SYSROOT_DIR)/usr/lib/crtn.o

$(USER_SMOKE_OBJECT): $(USER_SMOKE_SOURCE) userspace-sysroot
	@mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -MMD -MP -c $< -o $@

test:
	@echo "Tests are not wired yet. Add unit/integration targets in tests/."

check-posix:
	@echo "POSIX conformance tests are not wired yet."

docs:
	@echo "Documentation generation pipeline is not wired yet."

clean:
	$(RM) -r $(BUILD_DIR)

-include $(KERNEL_DEPS)
