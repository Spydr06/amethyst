PREFIX = /
BUILD_DIR = ./build

SOURCES := $(shell find -name '*.c')
OBJECTS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES))

TARGET := $(BUILD_DIR)/init

LDFLAGS += -static -l:libgetopt.a -lshard
CFLAGS += -D_AMETHYST_SRC

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	@echo "  CCLD  $@ ($^)"
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) 

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC    $^"
	$(CC) $(CFLAGS) -MMD -MP -MF "$(@:%.o=%.d)" -c -o $@ $^

$(PREFIX)/bin:
	mkdir $@

.PHONY: install
install: $(TARGET) $(PREFIX)/bin
	install -m "755" $(TARGET) $(PREFIX)/bin/init

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

