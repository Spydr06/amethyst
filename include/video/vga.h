#ifndef _AMETHYST_VIDEO_VGA_H
#define _AMETHYST_VIDEO_VGA_H

#define INIT_VGA_WIDTH 1280
#define INIT_VGA_HEIGHT 800

#define INIT_VGA_DEPTH 32

#ifndef ASM_FILE

#include <stdint.h>
#include <multiboot2.h>

struct vga {
    void* address;
    uint8_t bpp;
    uint32_t pitch;
    uint32_t memory_size;
    uint32_t width;
    uint32_t height;

    uintptr_t phys_addr;
};

void vga_init(const struct multiboot_tag_framebuffer* multiboot_info);
void vga_put_pixel(uint32_t x, uint32_t y, uint32_t color);

#endif /* ASM_FILE */

#endif /* _AMETHYST_VIDEO_VGA_H */

