#ifndef _AMETHYST_VIDEO_VGA_H
#define _AMETHYST_VIDEO_VGA_H

#ifndef ASM_FILE

#include <stdint.h>
#include <limine.h>

enum vga_mode : uint8_t {
    VGA_UNKNOWN = 0,
    VGA_RGBA_FRAMEBUFFER,
};

struct vga {
    enum vga_mode mode;
    void* address;
    void* framebuffer;
    uint8_t bpp;
    uint32_t pitch;
    uint32_t memory_size;
    uint32_t width;
    uint32_t height;

    uintptr_t phys_addr;
};

extern struct vga vga;
extern uint32_t vga_color_map[256];

extern volatile struct limine_framebuffer_request limine_fb_request;

int vga_init(void);

void vga_put_pixel(uint32_t x, uint32_t y, uint32_t color);
void vga_clear(uint32_t color);

#endif /* ASM_FILE */

#endif /* _AMETHYST_VIDEO_VGA_H */

