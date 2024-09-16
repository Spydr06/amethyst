ARCH ?= x86_64
VERSION ?= 0.0.1

BUILD_DIR ?= build
SHARD_BUILD_DIR ?= $(BUILD_DIR)/shard

SHARD_DIR ?= shard
SHARD_OBJECT ?= $(SHARD_BUILD_DIR)/libshard.o

LIMINE_DIR ?= limine
LIMINE_LOADERS := $(LIMINE_DIR)/limine-bios.sys $(LIMINE_DIR)/limine-bios-cd.bin $(LIMINE_DIR)/limine-uefi-cd.bin

KERNEL_ELF ?= $(BUILD_DIR)/amethyst-$(VERSION)-$(ARCH).elf
KERNEL_SYM ?= $(BUILD_DIR)/amethyst-$(VERSION)-$(ARCH).sym

ISOROOT_DIR ?= $(BUILD_DIR)/iso
ISO ?= amethyst.iso

SYSROOT_DIR ?= sysroot
INITRD ?= $(ISOROOT_DIR)/boot/initrd

TOOLPREFIX ?= $(ARCH)-elf-

override ARCH_DIR := arch/$(ARCH)
override SOURCE_DIRS := kernel init drivers $(ARCH_DIR)

SOURCE_PATTERN := -name "*.c" -or -name "*.cpp" -or -name "*.S" -or -name "*.ids"
SOURCES := $(shell find $(SOURCE_DIRS) $(SOURCE_PATTERN) | grep -v "arch/")

INCLUDES := . include arch/include $(SHARD_DIR)/libshard/include

SSP := $(shell openssl rand -hex 8)
CMOS_YEAR := $(shell date +"%Y")

override SAVED_CFLAGS := $(CFLAGS)
override SAVED_C_CXX_FLAGS := $(C_CXX_FLAGS)
override SAVED_LDFLAGS := $(LDFLAGS)

C_CXX_FLAGS += -Wall -Wextra -Wno-trigraphs \
			   -ffreestanding -fstack-protector -fno-lto -fPIE \
		       -g \
			   -D__$(ARCH)__ -D__$(ARCH) -D_SSP=0x$(SSP) -D_CMOS_YEAR=$(CMOS_YEAR) \
			   $(foreach i, $(INCLUDES), -I$(shell realpath $i))

override CC := $(TOOLPREFIX)gcc
override CXX := $(TOOLPREFIX)g++

override AS := $(TOOLPREFIX)as
ASFLAGS += -am -g

ifeq (, $(findstring gcc, $(CC)))
	$(error Unsupported C compiler "$(CC)", expect gcc)
endif

CC_VERSION := $(shell $(CC) -dumpversion)

ifneq ($(shell expr $(CC_VERSION) \>= 13),1)
_error:
	$(error Unsupported "$(CC)" version "$(CC_VERSION)", expect >= 13)
endif

override LDSCRIPT := $(ARCH_DIR)/link.ld

override LD := $(TOOLPREFIX)ld
LDFLAGS += -m elf_$(ARCH) -nostdlib \
		   -static -no-pie --no-dynamic-linker -z max-page-size=0x1000 \
		   -T $(LDSCRIPT)

override OBJCOPY := $(TOOLPREFIX)objcopy

CONSOLEFONT_OBJECT ?= $(BUILD_DIR)/default.psf.o
CONSOLEFONT ?= fonts/default.psf

TAR ?= tar

GDB ?= gdb
GDBFLAGS := -ex "target remote localhost:1234" \
			-ex "symbol-file $(KERNEL_SYM)"

override QEMU := qemu-system-$(ARCH)
QEMUFLAGS += -m 2G -serial stdio -smp cpus=1 -no-reboot -no-shutdown

ifeq ($(ARCH), x86_64)
	C_CXX_FLAGS += -m64 -march=x86-64 -mcmodel=large -mno-red-zone -mno-mmx -mno-sse2
	SOURCES += $(shell find $(ARCH_DIR) $(SOURCE_PATTERN))
else
_error:
	$(error Unsupported architecture "$(ARCH)")
endif

CFLAGS += -std=c2x $(C_CXX_FLAGS)
CXXFLAGS += -std=c++2x $(C_CXX_FLAGS)

OBJECTS := $(patsubst %, $(BUILD_DIR)/%.o, $(SOURCES))

VERSION_H := include/version.h

.PHONY: all
all: $(ISO)

.PHONY: kernel
kernel: $(KERNEL_ELF)

$(KERNEL_ELF): $(OBJECTS) $(CONSOLEFONT_OBJECT) $(SHARD_OBJECT)
	@echo "  LD    $@"
	@$(LD) $(LDFLAGS) $^ -o $@
	@$(OBJCOPY) --only-keep-debug $(KERNEL_ELF) $(KERNEL_SYM)
	@$(OBJCOPY) --strip-debug $(KERNEL_ELF)

$(BUILD_DIR)/%.c.o: %.c | $(VERSION_H)
	@mkdir -p $(dir $@)
	@echo "  CC    $^"
	@$(CC) $(CFLAGS) -MMD -MP -MF "$(@:%.c.o=%.c.d)" -c $^ -o $@

$(BUILD_DIR)/%.cpp.o: %.cpp | $(VERSION_H)
	@mkdir -p $(dir $@)
	@echo "  CXX   $^"
	@$(CXX) $(CXXFLAGS) -MMD -MP -MF "$(@:%.cpp.o=%.cpp.d)" -c $^ -o $@

$(BUILD_DIR)/%.S: %.S | $(VERSION_H)
	@mkdir -p $(dir $@)
	@echo "  GEN   $(notdir $@)"
	@$(CC) $(CFLAGS) -DASM_FILE -MMD -MP -MF "$(@:%.S=%.s.d)" -E $^ -o $@

$(BUILD_DIR)/%.S.o: $(BUILD_DIR)/%.S
	@mkdir -p $(dir $@)
	@echo "  AS    $^"
	@$(AS) $(ASFLAGS) -c $^ -o $@

$(BUILD_DIR)/%.ids.c: %.ids
	@mkdir -p $(dir $@)
	@echo "  MAKE  $^"
	@$(MAKE) -C $(dir $<) OUTPUT=$(shell realpath $(BUILD_DIR))/$<.c

$(BUILD_DIR)/%.ids.o: $(BUILD_DIR)/%.ids.c
	@echo "  CC    $^"
	@$(CC) $(CFLAGS) -MMD -MP -MF "$(@:%.ids.o=%.ids.d)" -c $^ -o $@

$(VERSION_H): $(VERSION_H).in
	@echo "  GEN   $@"
	@sed -e 's|@VERSION@|$(VERSION)|g' $< > $@

$(CONSOLEFONT_OBJECT): $(CONSOLEFONT)
	@echo "  LD    $^"
	@$(LD) -r -b binary -o $@ $<

$(SHARD_OBJECT): $(SHARD_DIR)
	@echo "  MAKE  $(SHARD_DIR)"
	@$(MAKE) -C $(SHARD_DIR) libshard_obj BUILD_DIR=$(shell realpath $(SHARD_BUILD_DIR)) \
		C_CXX_FLAGS="$(C_CXX_FLAGS)" CFLAGS="$(CFLAGS)" LDFLAGS="$(SAVED_LDFLAGS)" 

.PHONY: initrd
initrd: $(INITRD)

$(INITRD):
	@echo "  MAKE  $(SYSROOT_DIR)"
	@$(MAKE) -C $(SYSROOT_DIR) initrd INITRD=$(shell realpath $(shell pwd))/$(INITRD) BUILD_DIR=$(shell realpath $(BUILD_DIR))

.PHONY: iso
iso: $(ISO)

$(LIMINE_DIR):
	$(error "limine directory missing")

$(LIMINE_DIR)/limine: $(LIMINE_DIR)
	@echo "  MAKE  $(LIMINE_DIR)"
	@$(MAKE) -C $(LIMINE_DIR) limine CC=gcc LD=ld

$(ISO): $(KERNEL_ELF) | $(LIMINE_DIR)/limine $(INITRD) $(ISOROOT_DIR)/boot/limine/limine.cfg
	@echo "  CP    $< $(ISOROOT_DIR)/boot"
	@cp $< $(ISOROOT_DIR)/boot
	
	@echo "  CP    $(LIMINE_LOADERS) $(ISOROOT_DIR)/boot/limine"
	@cp $(LIMINE_LOADERS) $(ISOROOT_DIR)/boot/limine

	@echo "  MKDIR $(ISOROOT_DIR)/EFI/BOOT"
	@mkdir -p $(ISOROOT_DIR)/EFI/BOOT

	@echo "  CP    $(LIMINE_DIR)/BOOTX64.EFI $(ISOROOT_DIR)/EFI/BOOT"
	@cp $(LIMINE_DIR)/BOOTX64.EFI $(ISOROOT_DIR)/EFI/BOOT
	@echo "  CP    $(LIMINE_DIR)/BOOTIA32.EFI $(ISOROOT_DIR)/EFI_BOOT"
	@cp $(LIMINE_DIR)/BOOTIA32.EFI $(ISOROOT_DIR)/EFI_BOOT
	
	@echo "  XORRISO      $@"
	@xorriso -as mkisofs -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
        --efi-boot boot/limine/limine-uefi-cd.bin \
        -efi-boot-part --efi-boot-image --protective-msdos-label \
        $(ISOROOT_DIR) -o $@

	@echo "  BIOS-INSTALL $@"
	@$(LIMINE_DIR)/limine bios-install $@

$(ISOROOT_DIR)/boot/limine/limine.cfg: limine.cfg.in
	@mkdir -p $(dir $@)
	@echo "  GEN   $@"
	@sed -e 's|@VERSION@|$(VERSION)|g' \
		-e 's|@KERNEL_ELF@|$(notdir $(KERNEL_ELF))|' \
		$< > $@

.PHONY: run
run: $(ISO)
	$(QEMU) $(QEMUFLAGS) -cdrom $< -boot order=d

.PHONY: run-kvm
run-kvm: $(ISO)
	$(QEMU) $(QEMUFLAGS) -enable-kvm -cdrom $< -boot order=d

.PHONY: debug
debug: $(ISO)
	$(QEMU) $(QEMUFLAGS) -cdrom $< -boot order=d -s -S &
	$(GDB) $(GDBFLAGS)

.PHONY: test
test:
	$(MAKE) -C shard test

#.PHONY: shard
#shard:
#	C_CXX_FLAGS=$(SAVED_C_CXX_FLAGS) CFLAGS=$(SAVED_CFLAGS) LDFLAGS=$(SAVED_LDFLAGS) \
#		$(MAKE) -C shard BUILD_DIR=$(shell realpath $(SHARD_BUILD_DIR))

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(LIMINE_DIR)/boot
	rm -rf $(ISOROOT_DIR)
	rm -f $(VERSION_H)
