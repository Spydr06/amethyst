#include <multiboot2.h>
#include <kernelio.h>
#include <cdefs.h>

#include <stddef.h>
#include <stdint.h>

static const char* const tag_strings[] = {
    "END",
    "CMDLINE",
    "BOOT LOADER NAME",
    "MODULE",
    "BASIC MEMINFO",
    "BOOTDEV",
    "MMAP",
    "VBE",
    "FRAMEBUFFER",
    "ELF SECTIONS",
    "APM",
    "EFI32",
    "EFI64",
    "SMBIOS",
    "ACPI (old)",
    "ACPI (new)",
    "NETWORK",
    "EFI MMAP",
    "EFI BS",
    "EFI32 IH",
    "EFI64 IH",
    "LOAD BASE ADDR"
};

int parse_multiboot_tags(void (*handlers[])(const struct multiboot_tag*), size_t num_handlers) {
    if(multiboot_sig != MULTIBOOT2_BOOTLOADER_MAGIC) {
        printk("Corrupted multiboot information.\nGot: %p\n", (void*) (uintptr_t) multiboot_sig);
        return -1;
    }

    const uint8_t* ptr = __low_ptr(multiboot_ptr);
    
    struct boot_info {
        uint32_t total_size;
        uint32_t reserved;
    };

    const struct boot_info boot_info = *(struct boot_info*) ptr;
    printk("multiboot2 info at %p (size %u)\n", ptr, boot_info.total_size); 

    ptr += sizeof(struct boot_info);

    const struct multiboot_tag* tag;     
    size_t parsed = 0;

    while(1) {
        ptr = __align8(ptr);
        tag = (const struct multiboot_tag*) ptr;
        printk("  [%zu] %s (%u:%u)\n", parsed, tag_strings[tag->type], tag->type, tag->size);
        if(tag->type < num_handlers && handlers[tag->type])
            handlers[tag->type](tag);
        parsed++;
        if(tag->type == 0 && tag->size == 8)
            break;
        ptr += tag->size;
    }

    return parsed; 
}

