PREFIX ?= /

BUILD_DIR := build
INCLUDE_DIR := include
LIB_DIR := lib
CRT_DIR := crt

LIB_SOURCES := $(shell find $(LIB_DIR) -name '*.c' -or -name '*.S')
LIB_OBJECTS := $(patsubst %,$(BUILD_DIR)/%.o,$(LIB_SOURCES))

LIB_A := $(BUILD_DIR)/libc.a

CRT_SOURCES := $(shell find $(CRT_DIR) -name '*.c')
CRT_OBJECTS := $(patsubst $(CRT_DIR)/%.c,$(BUILD_DIR)/%.o,$(CRT_SOURCES))

CFLAGS += -I$(INCLUDE) -nostdlib -nostartfiles

NATIVE_GCC_SPECS := $(BUILD_DIR)/gcc/native-libc.specs
CROSS_GCC_SPECS := $(BUILD_DIR)/gcc/cross-libc.specs

TARGETS := $(LIB_A) $(CRT_OBJECTS) $(NATIVE_GCC_SPECS) $(CROSS_GCC_SPECS)

PUBLIC_HEADERS := $(shell find $(INCLUDE_DIR) -name '*.h')

INSTALL_TARGETS := $(patsubst $(BUILD_DIR)/%,$(PREFIX)/lib/%,$(TARGETS)) $(patsubst %,$(PREFIX)/%,$(PUBLIC_HEADERS))

CFLAGS += -nostdinc -nostdlib -nostartfiles -I$(INCLUDE_DIR) -I$(LIB_DIR)/$(INCLUDE_DIR) -Wall -Wextra -pedantic
LDFLAGS += -nostdlib -nostartfiles

.PHONY: all
all: $(TARGETS)

$(LIB_A): $(LIB_OBJECTS)
	@mkdir -p $(dir $@)
	@echo "  AR    $@ ($^)"
	@$(AR) rcs $@ $^

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.S.o: %.S
	@mkdir -p $(dir $@)
	@echo "  AS    $<"
	@$(AS) $(ASFLAGS) -o $@ $<

$(BUILD_DIR)/%.o: $(CRT_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(NATIVE_GCC_SPECS): libc-gcc.specs.in
	@mkdir -p $(dir $@)
	@echo "  GEN   $@"
	@sed -e 's|@PREFIX@|/|g' 		     \
		-e 's|@INCDIR@|$(INCLUDE_DIR)|g' \
		-e 's|@LIBDIR@|$(LIB_DIR)|g'     \
		$< > $@

$(CROSS_GCC_SPECS): libc-gcc.specs.in
	@mkdir -p $(dir $@)
	@echo "  GEN   $@"
	@sed -e 's|@PREFIX@|$(PREFIX)|g'     \
		-e 's|@INCDIR@|$(INCLUDE_DIR)|g' \
		-e 's|@LIBDIR@|$(LIB_DIR)|g'	 \
		$< > $@

.PHONY: install
install: $(INSTALL_TARGETS)
	@echo "  INSTALL include -> $(PREFIX)/include"
	@install -d include $(PREFIX)/include

$(PREFIX)/lib/%: $(BUILD_DIR)/%
	@mkdir -p $(dir $@)
	@echo "  INSTALL $< -> $@"
	@install -m 644 $< $@

$(PREFIX)/$(INCLUDE_DIR)/%: $(INCLUDE_DIR)/%
	@mkdir -p $(dir $@)
	@echo "  INSTALL $< -> $@"
	@install -m 644 $< $@

.PHONY: clean
clean:
	rm -rf $(TARGETS)
	rm -rf $(BUILD_DIR)
