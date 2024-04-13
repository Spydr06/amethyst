ARCH ?= x86_64
VERSION ?= 0.0.1

BUILD_DIR ?= build

KERNEL_ELF ?= $(BUILD_DIR)/amethyst-$(VERSION)-$(ARCH).elf
KERNEL_SYM ?= $(BUILD_DIR)/amethyst-$(VERSION)-$(ARCH).sym

ISOROOT_DIR ?= $(BUILD_DIR)/iso
ISO ?= amethyst.iso

SOURCE_PATTERN := -name "*.c" -or -name "*.cpp" -or -name "*.S" -or -name "*.s" 
SOURCES := $(shell find $(SOURCE_PATTERN) | grep -v "arch/")

INCLUDES := . include libk/include

SSP := $(shell openssl rand -hex 8)

C_CXX_FLAGS += -Wall -Wextra \
			   -ffreestanding -fno-stack-protector -fno-stack-check -fno-lto -fPIE \
		       -g \
			   -D__$(ARCH)__ -D__$(ARCH) -D__SSP__=0x$(SSP) \
			   $(foreach i, $(INCLUDES), -I$(shell realpath $i))

override CC := $(ARCH)-elf-gcc
CFLAGS += -std=c2x $(C_CXX_FLAGS)

override CXX := $(ARCH)-elf-g++
CXXFLAGS += -std=c++2x $(C_CXX_FLAGS)

override AS := $(ARCH)-elf-as
ASFLAGS += -am -g

override ARCH_DIR := arch/$(ARCH)
override LDSCRIPT := $(ARCH_DIR)/link.ld

override LD := $(ARCH)-elf-ld
LDFLAGS += -m elf_$(ARCH) -nostdlib \
		   -static -no-pie --no-dynamic-linker -z max-page-size=0x1000 \
		   -T $(LDSCRIPT)

override OBJCOPY := $(ARCH)-elf-objcopy

GDB ?= gdb
GDBFLAGS := -ex "target remote localhost:1234" \
			-ex "symbol-file $(KERNEL_SYM)"

override QEMU := qemu-system-$(ARCH)
QEMUFLAGS += -m 2G -serial stdio -display sdl

ifeq ($(ARCH), x86_64)
	CFLAGS += -m64 -march=x86-64 -mcmodel=large -mno-red-zone -mno-mmx -mno-sse2
	SOURCES += $(shell find $(ARCH_DIR) arch/x86-common $(SOURCE_PATTERN))
else
_error:
	$(error Unsupported architecture "$(ARCH)")
endif

OBJECTS := $(patsubst %, $(BUILD_DIR)/%.o, $(SOURCES))

VERSION_H := include/version.h

.PHONY: all
all: $(ISO)

.PHONY: kernel
kernel: $(KERNEL_ELF)

$(KERNEL_ELF): $(OBJECTS)
	$(LD) $(LDFLAGS) $^ -o $@
	$(OBJCOPY) --only-keep-debug $(KERNEL_ELF) $(KERNEL_SYM)
	$(OBJCOPY) --strip-debug $(KERNEL_ELF)

$(BUILD_DIR)/%.c.o: %.c | $(VERSION_H)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -MF "$(@:%.c.o=%.c.d)" -c $^ -o $@

$(BUILD_DIR)/%.cpp.o: %.cpp | $(VERSION_H)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -MF "$(@:%.cpp.o=%.cpp.d)" -c $^ -o $@

$(BUILD_DIR)/%.S: %.S | $(VERSION_H)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DASM_FILE -MMD -MP -MF "$(@:%.S=%.s.d)" -E $^ -o $@

$(BUILD_DIR)/%.S.o: $(BUILD_DIR)/%.S
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -c $^ -o $@

$(BUILD_DIR)/%.s.o: %.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -c $^ -o $@
 
$(VERSION_H): $(VERSION_H).in
	sed -e 's|@VERSION@|$(VERSION)|g' $< > $@

.PHONY: iso
iso: $(ISO)

$(ISO): $(KERNEL_ELF) | $(ISOROOT_DIR)/boot/grub/grub.cfg
	cp -v $< $(ISOROOT_DIR)/boot
	grub-mkrescue -o $@ $(ISOROOT_DIR)

$(ISOROOT_DIR)/boot/grub/grub.cfg: grub.cfg.in
	@mkdir -p $(dir $@)
	sed -e 's|@VERSION@|$(VERSION)|g' \
		-e 's|@KERNEL_ELF@|$(notdir $(KERNEL_ELF))|' \
		$< > $@

.PHONY: run
run: $(ISO)
	$(QEMU) $(QEMUFLAGS) -cdrom $< -boot order=d

.PHONY: debug
debug: $(ISO)
	$(QEMU) $(QEMUFLAGS) -cdrom $< -boot order=d -s -S &
	$(GDB) $(GDBFLAGS)

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(ISOROOT_DIR)
	rm -f $(VERSION_H)

