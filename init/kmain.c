#include <tty.h>
#include <multiboot2.h>
#include <kernelio.h>
#include <processor.h>

#include "cdefs.h"
#include "video/vga.h"
#include "init/interrupts.h"

#include <stdint.h>

void greet(void) {
    printk("Hello, World!\n");
}

uint32_t vga_buffer[INIT_VGA_WIDTH * INIT_VGA_HEIGHT];

//static struct vga vga;

static void init_vga(const struct multiboot_tag* tag) {
    const struct multiboot_tag_framebuffer* fb_tag = (struct multiboot_tag_framebuffer*) tag;

     klog(DEBUG, "Framebuffer of type %hhu at %p [%ux%u:%u]", 
        fb_tag->common.framebuffer_type, 
        (void*) fb_tag->common.framebuffer_addr,
        fb_tag->common.framebuffer_width, fb_tag->common.framebuffer_height, 
        fb_tag->common.framebuffer_bpp
    );

 //   vga_init(&vga, &fb_tag->common, vga_buffer);
}

static void (*multiboot_tag_handlers[])(const struct multiboot_tag*) = {
    [MULTIBOOT_TAG_TYPE_FRAMEBUFFER] = init_vga,
};

void kmain(void)
{
    console_init();
    init_interrupts();
    if(parse_multiboot_tags(multiboot_tag_handlers, __len(multiboot_tag_handlers)) < 0)
        panic("Failed parsing multiboot tags.");

//    vga_put_pixel(&vga, 10, 10, 0xff0000ff);
//    vga_buffer_to_screen(&vga);

    greet();
    hlt();
}
