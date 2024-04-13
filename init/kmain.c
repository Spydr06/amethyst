#include <stdint.h>
#include <tty.h>
#include <multiboot2.h>
#include <kernelio.h>
#include <processor.h>
#include <version.h>
#include <cdefs.h>
#include <video/console.h>
#include <video/vga.h>
#include <init/interrupts.h>
#include <mem/pmm.h>
#include <mem/mmap.h>

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

static void (*multiboot_tag_handlers[])(const struct multiboot_tag*) = {
    [MULTIBOOT_TAG_TYPE_FRAMEBUFFER] = init_vga,
    [MULTIBOOT_TAG_TYPE_MMAP] = init_mmap,
    [MULTIBOOT_TAG_TYPE_BASIC_MEMINFO] = init_basic_meminfo
};

static void greet(void) {
    printk("\n \e[1;32m>>\e[0m Booting \e[95mAmethyst\e[0m version \e[97m" AMETHYST_VERSION "\e[1;32m <<\e[0m\n");
}

static void color_test(void) {
    for(int i = 40; i <= 47; i++) {
        printk("\e[%im  \e[0m ", i);
    }
    for(int i = 100; i <= 107; i++) {
        printk("\e[%im  \e[0m ", i);
    }
    printk("\n\n");
}

void kmain(void)
{
    early_console_init();
    init_interrupts();
    size_t mbi_size = parse_multiboot_tags(multiboot_tag_handlers, __len(multiboot_tag_handlers));
    
    pmm_setup(multiboot_ptr, mbi_size);

    greet();
    color_test();

    hlt();
}

