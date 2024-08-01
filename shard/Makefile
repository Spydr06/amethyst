BUILD_DIR ?= build
LIBSHARD_DIR := libshard
GEODE_DIR := geode

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

GEODE_BIN := $(BUILD_DIR)/geode-host

TEST_RUNNER_SOURCE := tests/runner.c
TEST_RUNNER_BIN := $(BUILD_DIR)/tests/runner

LIBSHARD_SOURCES := $(shell find $(LIBSHARD_DIR) -name '*.c')
LIBSHARD_OBJECTS := $(patsubst %.c, $(BUILD_DIR)/%.o, $(LIBSHARD_SOURCES))

GEODE_SOURCES := $(shell find $(GEODE_DIR) -name '*.c')
GEODE_OBJECTS := $(patsubst %.c, $(BUILD_DIR)/%.o, $(GEODE_SOURCES))

AR ?= ar
RANLIB ?= ranlib

define unique
	$(strip $(if $1,$(firstword $1) $(call unique,$(filter-out $(firstword $1),$1))))
endef

override LDFLAGS +=
override LDFLAGS := $(call unique, $(LDFLAGS))

override CFLAGS += -Wall -Wextra -pedantic -std=c2x -I$(LIBSHARD_DIR)/include -g
override CFLAGS := $(call unique, $(CFLAGS))

ifneq (,$(findstring SHARD_USE_GCBOEHM, $(CFLAGS)))
	LIBSHARD_OBJECTS += $(GCBOEHM_LIB)
	CFLAGS += -I$(GCBOEHM_SRC_DIR)/include
endif

.PHONY: all
all: $(LIBSHARD) $(SHARD_BIN) $(GEODE_BIN)

.PHONY: lib
lib: $(LIBSHARD)

.PHONY: bin
bin: $(SHARD_BIN)

.PHONY: geode
geode: $(GEODE_BIN)

.PHONY: debug
debug: CFLAGS += -fsanitize=address,undefined -g
debug: LDFLAGS += -lasan -lubsan
debug: all

$(BUILD_DIR):
	@echo " MKDIR $(BUILD_DIR)"
	@mkdir -p $(BUILD_DIR)

$(SHARD_BIN): $(BUILD_DIR)/shard.o $(BUILD_DIR)/repl.o $(LIBSHARD_OBJ)
	@echo "  LD    $@"
	@$(CC) $(LDFLAGS) $@ -o $^

$(LIBSHARD): $(LIBSHARD_OBJ)
	@echo "  AR    $@"	
	@$(AR) -rcs $@ $<
	@echo " RANLIB	$@"
	@$(RANLIB) $@

$(GEODE_BIN): $(GEODE_OBJECTS) $(LIBSHARD_OBJ)
	@echo "  LD    $@"
	@$(CC) $(LDFLAGS) -o $@ $^

.PHONY: libshard_obj
libshard_obj: $(LIBSHARD_OBJ)

$(LIBSHARD_OBJ): $(LIBSHARD_OBJECTS) | $(BUILD_DIR)	
	@echo "  LD    $@"
	@$(LD) -r $^ -o $@

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@echo "  CC    $^"
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -MMD -MP -MF "$(@:%.o=%.d)" -c $^ -o $@

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

.PHONY: test
test: $(TEST_RUNNER_BIN)
	$(realpath $<) -d tests

.PHONY: test-runner
test-runner: $(TEST_RUNNER_BIN)

$(TEST_RUNNER_BIN): $(TEST_RUNNER_SOURCE) $(LIBSHARD_OBJ)
	@mkdir -p $(dir $@)
	@echo "  CC    $< -> $@"
	@$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^	

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: clean-gcboehm
clean-gcboehm: clean
	rm -rf $(LIBATOMIC_OPS_SRC_DIR) $(LIBATOMIC_OPS_TAR)
	rm -rf $(GCBOEHM_SRC_DIR) $(GCBOEHM_TAR)

