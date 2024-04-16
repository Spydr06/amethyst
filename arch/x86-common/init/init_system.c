#include <stdint.h>
#include <tty.h>
#include <multiboot2.h>
#include <kernelio.h>
#include <cpu/cpu.h>
#include <version.h>
#include <cdefs.h>
#include <drivers/video/console.h>
#include <drivers/video/vga.h>
#include <init/interrupts.h>
#include <mem/pmm.h>
#include <mem/mmap.h>
#include "../cpu/acpi.h"
#include "drivers/pci/pci.h"

extern void kmain(void);

static void init_vga(const struct multiboot_tag* tag) {
    const struct multiboot_tag_framebuffer* fb_tag = (struct multiboot_tag_framebuffer*) tag;

    klog(DEBUG, "Framebuffer of type %hhu at %p [%ux%u:%u]", 
        fb_tag->common.framebuffer_type, 
        (void*) fb_tag->common.framebuffer_addr,
        fb_tag->common.framebuffer_width, fb_tag->common.framebuffer_height, 
        fb_tag->common.framebuffer_bpp
    );

    vga_init(fb_tag);
    vga_console_init(VGACON_DEFAULT_OPTS);
}

static void init_mmap(const struct multiboot_tag* tag) { 
    mmap_parse((struct multiboot_tag_mmap*) tag);
}

static void init_basic_meminfo(const struct multiboot_tag* tag) {
    const struct multiboot_tag_basic_meminfo* mi_tag = (struct multiboot_tag_basic_meminfo*) tag;

    klog(INFO, "Available memory: %u kb - %u kb", mi_tag->mem_lower, mi_tag->mem_upper);
    memory_size_in_bytes = ((uintptr_t) mi_tag->mem_upper + 1024) * 1024;
}

static const struct multiboot_tag* acpi_tag = nullptr;
static void init_acpi(const struct multiboot_tag* tag) {
    acpi_tag = tag;
}

static void (*multiboot_tag_handlers[])(const struct multiboot_tag*) = {
    [MULTIBOOT_TAG_TYPE_FRAMEBUFFER] = init_vga,
    [MULTIBOOT_TAG_TYPE_MMAP] = init_mmap,
    [MULTIBOOT_TAG_TYPE_BASIC_MEMINFO] = init_basic_meminfo,
    [MULTIBOOT_TAG_TYPE_ACPI_OLD] = init_acpi,
    [MULTIBOOT_TAG_TYPE_ACPI_NEW] = init_acpi,
};

__noreturn void _init_basic_system(void)
{
    early_console_init();
    init_interrupts();
    size_t mbi_size = parse_multiboot_tags(multiboot_tag_handlers, __len(multiboot_tag_handlers));
    
    pmm_setup(multiboot_ptr, mbi_size);
    
    if(!acpi_tag)
        panic("No ACPI tag received from bootloader.");
    if(acpi_tag->type == MULTIBOOT_TAG_TYPE_ACPI_OLD)
        acpi_parse_sdt((uintptr_t) (((struct multiboot_tag_old_acpi*) acpi_tag) + 1), MULTIBOOT_TAG_TYPE_ACPI_OLD);
    else
        acpi_parse_sdt((uintptr_t) (((struct multiboot_tag_new_acpi*) acpi_tag) + 1), MULTIBOOT_TAG_TYPE_ACPI_NEW);

    pci_init(0x0cfc, 0x0cf8);

    kmain();
    hlt();
}
