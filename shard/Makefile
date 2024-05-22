BUILD_DIR := build
LIBSHARD_DIR := libshard

SHARD_BIN := build/shard
LIBSHARD := build/libshard.a

SOURCES := $(shell find $(LIBSHARD_DIR) -name '*.c')
OBJECTS := $(patsubst %.c, $(BUILD_DIR)/%.o, $(SOURCES))

AR ?= ar
RANLIB ?= ranlib

LDFLAGS +=
CFLAGS += -Wall -Wextra -pedantic -std=c2x -I$(LIBSHARD_DIR)/include

.PHONY: all
all: $(LIBSHARD) $(SHARD_BIN)

.PHONY: lib
lib: $(LIBSHARD)

.PHONY: bin
bin: $(SHARD_BIN)

.PHONY: debug
debug: CFLAGS += -fsanitize=address,undefined -g
debug: LDFLAGS += -lasan -lubsan
debug: all

$(BUILD_DIR):
	@echo " MKDIR $(BUILD_DIR)"
	@mkdir -p $(BUILD_DIR)

$(SHARD_BIN): $(OBJECTS) $(BUILD_DIR)/shard.o
	@echo " LD    $@"
	@$(CC) $(LDFLAGS) $^ -o $@

$(LIBSHARD): $(OBJECTS) | $(BUILD_DIR)
	@echo " AR    $@"	
	@$(AR) -ru $@ $<
	@echo " RANLIB	$@"
	@$(RANLIB) $@

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@echo " CC    $^"
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -MMD -MP -MF "$(@:%.o=%.d)" -c $^ -o $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
