PREFIX ?= /
BUILD_DIR ?= build

LIBSHARD_DIR := libshard
GEODE_DIR := geode
SHELL_DIR := shell

LIBATOMIC_OPS_TAR_LINK ?= https://www.hboehm.info/gc/gc_source/libatomic_ops-7.8.0.tar.gz
LIBATOMIC_OPS_TAR := $(shell basename $(LIBATOMIC_OPS_TAR_LINK))
LIBATOMIC_OPS_SRC_DIR := $(patsubst %.tar.gz, %, $(LIBATOMIC_OPS_TAR))

GCBOEHM_TAR_LINK ?= https://www.hboehm.info/gc/gc_source/gc-8.2.4.tar.gz
GCBOEHM_TAR := $(shell basename $(GCBOEHM_TAR_LINK))
GCBOEHM_SRC_DIR := $(patsubst %.tar.gz, %, $(GCBOEHM_TAR))

GCBOEHM_LIB := $(GCBOEHM_SRC_DIR)/.libs/libgc.a

SHARD_BIN := $(BUILD_DIR)/shard
LIBSHARD := $(BUILD_DIR)/libshard.a
LIBSHARD_OBJ := $(BUILD_DIR)/libshard.o

LIBSHARD_H := libshard/include/libshard.h

LIBSHARD_SOURCES := $(shell find $(LIBSHARD_DIR) -name '*.c')
LIBSHARD_OBJECTS := $(patsubst %.c, $(BUILD_DIR)/%.o, $(LIBSHARD_SOURCES))

SHARDBIN_SOURCES := $(wildcard *.c)
SHARDBIN_OBJECTS := $(patsubst %.c, $(BUILD_DIR)/%.o, $(SHARDBIN_SOURCES))

TEST_RUNNER_SOURCE := tests/runner.c
TEST_RUNNER_BIN := $(BUILD_DIR)/tests/runner

GEODE_BIN := $(BUILD_DIR)/geode-bin

GEODE_LIBS := libarchive libcurl libgit2

GEODE_SOURCES := $(shell find $(GEODE_DIR) -name '*.c' -or -name '*.S')
GEODE_OBJECTS := $(patsubst %.S, $(BUILD_DIR)/%.o, $(patsubst %.c, $(BUILD_DIR)/%.o, $(GEODE_SOURCES)))

GEODE_CFLAGS := -I$(GEODE_DIR)/include $(shell pkg-config --cflags $(GEODE_LIBS)) -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
GEODE_LDFLAGS := $(shell pkg-config --libs $(GEODE_LIBS)) -rdynamic

SHELL_BIN := $(BUILD_DIR)/shard-sh

SHELL_SOURCES := $(shell find $(SHELL_DIR) -name '*.c' -or -name '*.S')
SHELL_OBJECTS := $(patsubst %.S, $(BUILD_DIR)/%.o, $(patsubst %.c, $(BUILD_DIR)/%.o, $(SHELL_SOURCES)))

SHELL_CLFAGS := -I$(SHELL_DIR)/include -g
SHELL_LDFLAGS := -ledit

LIBC_DRIVER := $(BUILD_DIR)/shard_libc_driver.o
LIBC_DRIVER_SOURCE := shard_libc_driver.c

AR ?= ar
RANLIB ?= ranlib

define unique
	$(strip $(if $1,$(firstword $1) $(call unique,$(filter-out $(firstword $1),$1))))
endef

override LDFLAGS := $(call unique, $(LDFLAGS))

override CFLAGS += -Wall -Wextra -std=c2x -I$(LIBSHARD_DIR)/include
override CFLAGS := $(call unique, $(CFLAGS))

ifneq (,$(findstring SHARD_USE_GCBOEHM, $(CFLAGS)))
	LIBSHARD_OBJECTS += $(GCBOEHM_LIB)
	override CFLAGS += -I$(GCBOEHM_SRC_DIR)/include
endif

.PHONY: all
all: $(LIBSHARD) $(SHARD_BIN) $(GEODE_BIN) $(SHELL_BIN)

.PHONY: lib
lib: $(LIBSHARD)

.PHONY: bin
bin: $(SHARD_BIN)

.PHONY: geode
geode: $(GEODE_BIN)

.PHONY: shell
shell: $(SHELL_BIN)

.PHONY: debug
debug: CFLAGS += -fsanitize=address,undefined -g
debug: LDFLAGS += -lasan -lubsan
debug: all

$(BUILD_DIR):
	@echo "  MKDIR $(BUILD_DIR)"
	@mkdir -p $(BUILD_DIR)

$(SHARD_BIN): $(SHARDBIN_OBJECTS) $(LIBSHARD_OBJ) $(LIBC_DRIVER)
	@echo "  LD    $@"
	@$(CC) $(LDFLAGS) -o $@ $^ -ledit

$(LIBSHARD): $(LIBSHARD_OBJ)
	@echo "  AR    $@"	
	@$(AR) -rcs $@ $<
	@echo " RANLIB	$@"
	@$(RANLIB) $@

$(GEODE_BIN): $(GEODE_OBJECTS) $(LIBSHARD_OBJ) $(LIBC_DRIVER)
	@echo "  CCLD  $@"
	@$(CC) $(LDFLAGS) $(GEODE_LDFLAGS) -o $@ $^

$(SHELL_BIN): $(SHELL_OBJECTS) $(LIBSHARD_OBJ) $(LIBC_DRIVER)
	@echo "  CCLD  $@"
	@$(CC) $(CFLAGS) $(SHELL_LDFLAGS) -Wl,-z,noexecstack -o $@ $^ $(LDFLAGS)

.PHONY: libshard_obj
libshard_obj: $(LIBSHARD_OBJ)

$(LIBSHARD_OBJ): $(LIBSHARD_OBJECTS) | $(BUILD_DIR)	
	@echo "  LD    $@"
	@$(LD) -r $^ -o $@

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@echo "  CC    $^"
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) \
		$(if $(filter $(GEODE_DIR)/%,$<),$(GEODE_CFLAGS)) \
		$(if $(filter $(SHELL_DIR)/%,$<),$(SHELL_CLFAGS)) \
		-MMD -MP -MF "$(@:%.o=%.d)" -c $^ -o $@

$(BUILD_DIR)/%.o: %.S | $(BUILD_DIR)
	@echo "  AS    $^"
	@mkdir -p $(dir $@)
	@$(AS) $(ASFLAGS) $^ -o $@

.PHONY: gcboehm
gcboehm: $(GCBOEHM_LIB)

$(GCBOEHM_LIB): $(GCBOEHM_SRC_DIR)/Makefile
	$(MAKE) -C $(GCBOEHM_SRC_DIR)

$(GCBOEHM_SRC_DIR)/Makefile: $(GCBOEHM_SRC_DIR)/libatomic_ops
	@echo "  PUSHD $(GCBOEHM_SRC_DIR)"		&& \
		pushd $(GCBOEHM_SRC_DIR) 			&& \
		echo "Configuring gcboehm..." 		&& \
		autoreconf -vif 					&& \
		automake --add-missing 				&& \
		CC=$(CC) CXX=$(CXX)					   \
		./configure --enable-static=yes --enable-shared=no		&& \
		echo "Configuring gcboehm done."

$(GCBOEHM_SRC_DIR)/libatomic_ops: $(LIBATOMIC_OPS_SRC_DIR) | $(GCBOEHM_SRC_DIR)
	@echo "  LN[S] $@"
	@ln -s $(shell realpath $^) $@

$(GCBOEHM_SRC_DIR): $(GCBOEHM_TAR)
	@echo "  TAR   $^"
	@tar xf $^ -C $(dir $(GCBOEHM_SRC_DIR))

$(GCBOEHM_TAR):
	@echo "  WGET  $@"
	@wget $(GCBOEHM_TAR_LINK) -O $@

$(LIBATOMIC_OPS_TAR):
	@echo "  WGET  $@"
	@wget $(LIBATOMIC_OPS_TAR_LINK) -O $@

$(LIBATOMIC_OPS_SRC_DIR): $(LIBATOMIC_OPS_TAR)
	@echo "  TAR   $^"
	@tar xf $^ -C $(dir $(LIBATOMIC_OPS_SRC_DIR))

.PHONY: libc-driver
libc-driver: $(LIBC_DRIVER)

.PHONY: test
test: $(TEST_RUNNER_BIN)
	$(realpath $<) -d tests

.PHONY: test-runner
test-runner: $(TEST_RUNNER_BIN)

$(TEST_RUNNER_BIN): $(TEST_RUNNER_SOURCE) $(LIBSHARD_OBJ) $(LIBC_DRIVER)
	@mkdir -p $(dir $@)
	@echo "  CC    $< -> $@"
	@$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^	

.PHONY: install
install: install-lib

.PHONY: install-lib
install-lib: $(LIBSHARD)
	install -m "644" $(LIBSHARD) $(PREFIX)/lib/$(notdir $(LIBSHARD))
	install -m "644" $(LIBSHARD_H) $(PREFIX)/include/$(notdir $(LIBSHARD_H))

.PHONY: install-bin
install-bin: $(SHARD_BIN)
	install -m "777" $(SHARD_BIN) $(PREFIX)/bin/$(notdir $(SHARD_BIN))

.PHONY: install-shell
install-shell: $(SHELL_BIN)
	install -m "777" $(SHELL_BIN) $(PREFIX)/bin/$(notdir $(SHELL_BIN))

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: clean-gcboehm
clean-gcboehm: clean
	rm -rf $(LIBATOMIC_OPS_SRC_DIR) $(LIBATOMIC_OPS_TAR)
	rm -rf $(GCBOEHM_SRC_DIR) $(GCBOEHM_TAR)

