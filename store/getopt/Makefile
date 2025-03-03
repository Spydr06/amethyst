PREFIX ?= /

BUILD_DIR := build
INCLUDE_DIR := include

SOURCES := $(shell find -name '*.c')
OBJECTS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES))

LIBGETOPT := $(BUILD_DIR)/libgetopt.a

CFLAGS += -I$(INCLUDE_DIR)

.PHONY: all
all: $(LIBGETOPT)

$(LIBGETOPT): $(OBJECTS)
	$(AR) rcs $@ $^

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: install
install:
	install -m 644 $(LIBGETOPT) $(PREFIX)/lib/$(notdir $(LIBGETOPT))

$(BUILD_DIR):
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

