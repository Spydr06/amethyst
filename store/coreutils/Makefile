PREFIX = /
BUILD_DIR = ./build
INSTALL_DIR = $(PREFIX)/bin
LIB_DIR = $(PREFIX)/lib

COMMON_DIR := src
COMMON_SOURCES := $(shell find $(COMMON_DIR) -name '*.c')
COMMON_OBJECTS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(COMMON_SOURCES))
COMMON_ARCHIVE := $(BUILD_DIR)/libcoreutil.a

TARGET_SOURCES := $(wildcard *.c)
TARGET_OBJECTS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(TARGET_SOURCES))
TARGETS        := $(patsubst %.o,%,$(TARGET_OBJECTS))

INSTALL_TARGETS := $(patsubst $(BUILD_DIR)/%,$(INSTALL_DIR)/%,$(TARGETS))

.PHONY: all
all: $(TARGETS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC    $^"
	@$(CC) $(CFLAGS) -ggdb -MMD -MP -MF "$(@:%.o=%.d)" -c -o $@ $^

$(COMMON_ARCHIVE): $(COMMON_OBJECTS)
	@echo "  AR    $@ ($^)"
	@$(AR) rcs $@ $^

$(BUILD_DIR)/%: $(BUILD_DIR)/%.o $(COMMON_ARCHIVE)
	@echo "  CCLD  $@ ($^)"
	@$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

.PHONY: install
install: $(INSTALL_TARGETS)

$(PREFIX)/bin/%: $(BUILD_DIR)/%
	@echo "  INSTALL $< -> $@"
	@install -m "755" $< $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

