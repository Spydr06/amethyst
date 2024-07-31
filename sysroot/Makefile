BUILD_DIR ?= build
INITRD ?= $(BUILD_DIR)/initrd

INITRD_DIR ?= $(BUILD_DIR)/initrd-files

INITRD_SRC_FILES += test.txt bin/init
override INITRD_DST_FILES := $(addprefix $(INITRD_DIR)/, $(INITRD_SRC_FILES))

.PHONY: all
all: initrd

$(INITRD_DIR):
	@echo "  MKDIR $(INITRD_DIR)"
	@mkdir -p $(INITRD_DIR)

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
