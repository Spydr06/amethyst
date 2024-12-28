PREFIX ?=

BUILD_DIR := build

LIB_DIR := lib

LIB_SOURCES := $(shell find lib -name '*.c' -or -name '*.S')
LIB_OBJECTS := $(patsubst %,$(BUILD_DIR)/%.o,$(LIB_SOURCES))

LIB_OBJECT := libc.o

CFLAGS += -Iinclude -nostdlib -nostartfiles

.PHONY: all
all: $(LIB_OBJECT)

$(LIB_OBJECT): $(LIB_OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $<

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.S.o: %.S
	@mkdir -p $(dir $@)
	@echo "  AS    $<"
	@$(AS) $(ASFLAGS) -o $@ $<

.PHONY: install
install: $(LIB_OBJECT)
	install -m 666 $(LIB_OBJECT) $(PREFIX)/lib
	install -d include $(PREFIX)/usr/include

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
