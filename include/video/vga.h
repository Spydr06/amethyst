#ifndef _AMETHYST_VIDEO_VGA_H
#define _AMETHYST_VIDEO_VGA_H

#define INIT_VGA_WIDTH 1280
#define INIT_VGA_HEIGHT 720

#define INIT_VGA_DEPTH 32

#ifndef ASM_FILE

#include <stdint.h>
#include <multiboot2.h>

struct vga {
    uint32_t width;
    uint32_t height;
    uint32_t* buffer;
    uint32_t* screen;
};

void vga_init(struct vga* vga, const struct multiboot_tag_framebuffer_common* multiboot_info, uint32_t* buffer);
void vga_put_pixel(struct vga* vga, uint32_t x, uint32_t y, uint32_t color);
void vga_buffer_to_screen(struct vga* vga);

#endif /* ASM_FILE */

#endif /* _AMETHYST_VIDEO_VGA_H */

