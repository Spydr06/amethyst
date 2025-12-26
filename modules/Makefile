ARCH ?= x86_64

ifndef MODULE_DIR
_error:
	$(error MODULE_DIR is not set)
endif

MODULE_NAME ?= $(notdir $(MODULE_DIR))
BUILD_DIR ?= $(MODULE_DIR)/build

MODULE_TARGET ?= $(MODULE_DIR)/$(MODULE_NAME).ko
MODULE_SOURCES := $(shell find $(MODULE_DIR) -name '*.c' -or -name '*.cpp' -or -name '*.S')
MODULE_HEADERS := $(shell find $(MODULE_DIR) -name '*.h' -or -name '*.hpp')

TOOLPREFIX ?= $(ARCH)-elf-

C_CXX_FLAGS += -Wall -Wextra -Wno-trigraphs \
			   -ffreestanding -fstack-protector -fno-lto -fPIC \
			   -g \
			   -D__$(ARCH)__ -D__$(ARCH) -D_AMETHYST_MODULE_SRC

override CC := $(TOOLPREFIX)gcc
override CXX := $(TOOLPREFIX)g++

override AS := $(TOOLPREFIX)as
ASFLAGS += -am -g

ifeq (, $(findstring gcc, $(CC)))
_error:
	$(error Unsupported C compiler "$(CC)", expect gcc)
endif

CC_VERSION := $(shell $(CC) -dumpversion)

ifneq ($(shell expr $(CC_VERSION) \>= 14),1)
_error:
	$(error Unsupported "$(CC)" version "$(CC_VERSION)", expect >= 14)
endif

LDSCRIPT := ./default.ld

override LD := $(TOOLPREFIX)ld
LDFLAGS += -m elf_$(ARCH) -nostdlib \
		   -static -no-pie --no-dynamic-linker -z max-page-size=0x1000 \
		   -T $(LDSCRIPT)

ifeq ($(ARCH), x86_64)
	C_CXX_FLAGS += -m64 -march=x86-64 -mcmodel=large -mno-red-zone -mno-mmx -mno-sse2
else
_error:
	$(error Unsupported architecture "$(ARCH)")
endif

CFLAGS += -std=c2x $(C_CXX_FLAGS)
CXXFLAGS += -std=c++2x $(C_CXX_FLAGS)

MODULE_OBJECTS := $(patsubst $(MODULE_DIR)/%, $(BUILD_DIR)/%.o, $(MODULE_SOURCES))

.PHONY: all
all: module

.PHONY: module
module: $(MODULE_TARGET)

$(MODULE_TARGET): $(MODULE_OBJECTS) | modulemake
	@echo "  LD    $@"
	@$(LD) $(LDFLAGS) -r -o $@ $^

$(BUILD_DIR)/%.c.o: $(MODULE_DIR)/%.c $(MODULE_HEADERS)
	@mkdir -p $(dir $@)
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -MMD -MP -MF "$(@:%.c.o=%.c.d)" -c $< -o $@

$(BUILD_DIR)/%.cpp.o: $(MODULE_DIR)/%.cpp $(MODULE_HEADERS)
	@mkdir -p $(dir $@)
	@echo "  CXX   $^"
	@$(CXX) $(CFLAGS) -MMD -MP -MF "$(@:%.cpp.o=%.cpp.d)" -c $< -o $@

$(BUILD_DIR)/%.S.o: $(MODULE_DIR)/%.S
	@mkdir -p $(dir $@)
	@echo "  AS    $<"
	@$(AS) $(ASFLAGS) -c $< -o $@

.PHONY: modulemake
modulemake: $(MODULE_DIR)
ifneq (,$(wildcard $</Makefile))
	@echo "  MAKE  $<"
	@$(MAKE) -C $<
endif

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(MODULE_TARGET)

