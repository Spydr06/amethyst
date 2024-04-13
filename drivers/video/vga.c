#include "ff/psf.h"
#include <cdefs.h>
#include <stdint.h>
#include <video/vga.h>

#ifdef __x86_64__
    #include <arch/x86_64/paging.h>
#endif

struct vga vga = {0};

void vga_init(const struct multiboot_tag_framebuffer* tag) {
    vga.address = (void*)(uint64_t) FRAMEBUFFER_MEM_START;
    vga.pitch = tag->common.framebuffer_pitch;
    vga.bpp = tag->common.framebuffer_bpp;
    vga.memory_size = tag->common.framebuffer_pitch * tag->common.framebuffer_height;
    vga.width = tag->common.framebuffer_width;
    vga.height = tag->common.framebuffer_height;
    vga.phys_addr = tag->common.framebuffer_addr;

    __framebuffer_map_page(tag);
}

void vga_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    *((uint32_t*) (vga.address + (y * vga.pitch) + (x * sizeof(uint32_t)))) = color;
}

void vga_clear(uint32_t color) {
    uint32_t* fb = vga.address;    
    for(size_t i = 0; i < vga.memory_size / sizeof(uint32_t); i++)
        fb[i] = color;
}

void vga_putchar(char c, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg, uint32_t width, uint32_t height) {
    uint8_t* glyph = psf_get_glyph(c, _binary_fonts_default_psf_start);

    size_t bytesperline = (width + 7) / 8;
    size_t offset = (y * height * vga.pitch) + (x * width * sizeof(uint32_t));

    uint32_t xx, line;
    for(uint32_t y = 0; y < height; y++) {
        line = offset;
        for(xx = 0; xx < width; xx++) {
            *((uint32_t*) (vga.address + line)) = glyph[xx / 8] & (0x80 >> (xx & 7)) ? fg : bg;
            line += vga.bpp / 8;
        }

        glyph += bytesperline;
        offset += vga.pitch;
    }
}

