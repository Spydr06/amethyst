KERNEL_ELF ?= kernel.elf
KERNEL_SYM ?= kernel.sym
BUILD_DIR ?= build
ISOROOT_DIR ?= $(BUILD_DIR)/iso

ISO ?= amethyst.iso

SOURCES := $(shell find -name "*.c" -or -name "*.S" | grep -v "arch/")

ARCH ?= x86_64

override CC := $(ARCH)-elf-gcc
CFLAGS += -Wall -Wextra -std=c2x\
		  -ffreestanding -fno-stack-protector -fno-stack-check -fno-lto -fPIE \
		  -I$(shell realpath include) -I$(shell realpath .) -g

override AS := $(ARCH)-elf-as
ASFLAGS += -am

override ARCH_DIR := arch/$(ARCH)
override LDSCRIPT := $(ARCH_DIR)/link.ld

override LD := $(ARCH)-elf-ld
LDFLAGS += -m elf_$(ARCH) -nostdlib -static -no-pie --no-dynamic-linker -z max-page-size=0x1000 -T $(LDSCRIPT)

override OBJCOPY := $(ARCH)-elf-objcopy

override QEMU := qemu-system-$(ARCH)
QEMUFLAGS += -m 2G -serial stdio -display sdl

ifeq ($(ARCH), x86_64)
	CFLAGS += -m64 -march=x86-64 -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2
	SOURCES += $(shell find $(ARCH_DIR) arch/x86-common -name "*.c" -or -name "*.S")
else
_error:
	$(error Unsupported architecture "$(ARCH)")
endif

OBJECTS := $(patsubst %, $(BUILD_DIR)/%.o, $(SOURCES))

.PHONY: all
all: $(ISO)

.PHONY: kernel
kernel: $(KERNEL_ELF)

$(KERNEL_ELF): $(OBJECTS)
	$(LD) $(LDFLAGS) $^ -o $@
	$(OBJCOPY) --only-keep-debug $(KERNEL_ELF) $(KERNEL_SYM)
	$(OBJCOPY) --strip-debug $(KERNEL_ELF)

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -MF "$(@:%.c.o=%.c.d)" -c $^ -o $@

$(BUILD_DIR)/%.S: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -MF "$(@:%.S=%.s.d)" -E $^ -o $@

$(BUILD_DIR)/%.S.o: $(BUILD_DIR)/%.S
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -c $^ -o $@

.PHONY: iso
iso: $(ISO)

$(ISO): $(KERNEL_ELF)
	@mkdir -p $(ISOROOT_DIR)/boot/grub
	cp -v grub.cfg $(ISOROOT_DIR)/boot/grub
	cp -v $< $(ISOROOT_DIR)/boot
	grub-mkrescue -o $@ $(ISOROOT_DIR)

.PHONY: run
run: $(ISO)
	$(QEMU) $(QEMUFLAGS) -cdrom $< -boot order=d -enable-kvm

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(KERNEL_ELF)
	rm -f $(KERNEL_SYM)
	rm -rf $(ISOROOT_DIR)

