BUILD_DIR ?= build
INITRD ?= $(BUILD_DIR)/initrd

INITRD_DIR ?= $(BUILD_DIR)/initrd-files

INITRD_SRC_FILES += test.txt bin/init
override INITRD_DST_FILES := $(addprefix $(INITRD_DIR)/, $(INITRD_SRC_FILES))

override LD := x86_64-elf-ld
override AS := x86_64-elf-as

.PHONY: all
all: initrd

$(INITRD_DIR):
	@echo "  MKDIR $(INITRD_DIR)"
	@mkdir -p $(INITRD_DIR)

$(BUILD_DIR)/bin:
	@echo "  MKDIR $(BUILD_DIR)/bin"
	@mkdir -p $(BUILD_DIR)/bin

$(BUILD_DIR)/bin/init.o: bin/init.S | $(BUILD_DIR)/bin
	@echo "  AS   $<"
	@$(AS) -o $@ $<

$(BUILD_DIR)/bin/init: $(BUILD_DIR)/bin/init.o
	@echo "  LD   $@"
	@$(LD) -static -o $@ $<

bin/init: $(BUILD_DIR)/bin/init
	@echo "  CP   $< -> $@"
	@cp $< $@

$(INITRD_DST_FILES): $(INITRD_DIR)/% : %
	@echo "  CP    $< -> $@"
	@[[ -e $< ]] || (echo "error copying $<" && false)
	@mkdir -p $(dir $@)
	@cp $< $@

.PHONY: initrd
initrd: $(INITRD)

$(INITRD): $(INITRD_DST_FILES)
	@mkdir -p $(dir $(INITRD))
	
	@echo "pushd: Entering directory $(INITRD_DIR)"  && \
		pushd $(INITRD_DIR)							 && \
		echo "  TAR   $(INITRD_DIR) -> $(INITRD)"	 && \
		tar --format=ustar -cf $(INITRD) * 			 && \
		echo "popd: Leaving directory $(INITRD_DIR)" && \
		popd

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	rm -f bin/init
