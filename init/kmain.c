#include <tty.h>
#include <multiboot2.h>
#include <kernelio.h>
#include <processor.h>
#include <version.h>

#include "cdefs.h"
#include "video/console.h"
#include "video/vga.h"
#include "init/interrupts.h"

void greet(void) {
    printk(" \e[1;32m*\e[0m Booting \e[35mAmethyst\e[0m version \e[97m" AMETHYST_VERSION "\e[0m\n");
}

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

static void (*multiboot_tag_handlers[])(const struct multiboot_tag*) = {
    [MULTIBOOT_TAG_TYPE_FRAMEBUFFER] = init_vga,
};

void kmain(void)
{
    early_console_init();
    init_interrupts();
    if(parse_multiboot_tags(multiboot_tag_handlers, __len(multiboot_tag_handlers)) < 0)
        panic("Failed parsing multiboot tags.");

    greet();

    hlt();
}
