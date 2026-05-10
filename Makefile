ARCH ?= x86_64

-include config.mk

ifeq ($(ARCH),x86_64)
TARGET ?= x86_64-elf
QEMU_SYSTEM ?= qemu-system-x86_64
KERNEL_LINKER_SCRIPT := kernel/arch/x86_64/linker.ld
ARCH_C_SOURCES := kernel/arch/x86_64/serial.c
ARCH_C_SOURCES += kernel/arch/x86_64/interrupts/pic.c
ARCH_C_SOURCES += kernel/arch/x86_64/timer/pit.c
ARCH_ASM_SOURCES := kernel/arch/x86_64/boot/multiboot2_header.S \
	kernel/arch/x86_64/boot/entry.S \
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

COMMON_CFLAGS := -std=$(CSTD) -O2 -g -ffreestanding -fno-stack-protector -fno-pie \
	-mno-red-zone -mgeneral-regs-only -Wall -Wextra -Wpedantic -Iinclude -Ikernel/include
COMMON_LDFLAGS := -nostdlib -no-pie -Wl,--build-id=none,-n,-T,$(KERNEL_LINKER_SCRIPT)

KERNEL_C_SOURCES := \
	kernel/init/kmain.c \
	kernel/init/panic.c \
	kernel/arch/x86_64/interrupts/idt.c \
	kernel/mm/pmm.c \
	kernel/mm/vmm.c \
	kernel/sched/sched.c \
	kernel/sched/workqueue.c \
	kernel/lib/spinlock.c \
	kernel/lib/mutex.c \
	kernel/lib/rwlock.c \
	kernel/lib/printk.c \
	kernel/arch/x86_64/boot/multiboot2.c

KERNEL_OBJECTS := \
	$(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_C_SOURCES) $(ARCH_C_SOURCES)) \
	$(patsubst %.S,$(BUILD_DIR)/%.o,$(ARCH_ASM_SOURCES))
KERNEL_DEPS := $(KERNEL_OBJECTS:.o=.d)

.PHONY: all kernel userspace image iso run run-debug test check-posix docs clean

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

userspace:
	@echo "Userspace build pipeline is not implemented yet."

test:
	@echo "Tests are not wired yet. Add unit/integration targets in tests/."

check-posix:
	@echo "POSIX conformance tests are not wired yet."

docs:
	@echo "Documentation generation pipeline is not wired yet."

clean:
	$(RM) -r $(BUILD_DIR)

-include $(KERNEL_DEPS)
