#ifndef _AMETHYST_SYS_FB_H
#define _AMETHYST_SYS_FB_H

#include <inttypes.h>

enum fb_type : uint16_t {
    FB_TYPE_PACKED_PIXELS,
    FB_TYPE_PLANES,
    FB_TYPE_INTERLEAVED_PLANES,
    FB_TYPE_TEXT,
    FB_TYPE_VGA_PLANES
};

enum fb_visual : uint16_t {
    FB_VISUAL_TRUECOLOR
};

enum fb_ioctl_request : uint64_t {
    FBIOGET_VSCREENINFO = 0x4600,
    FBIOGET_FSCREENINFO = 0x4602
};  

// similar to fb_fix_screeninfo in <linux/fb.h>
struct fb_fix_screeninfo {
    char id[16];
    
    void* smem_start;
    uint32_t smem_len;

    enum fb_type type;
    enum fb_visual visual;

    uint32_t line_length;
};

struct fb_bitfield {
    uint32_t offset;
    uint32_t length;
    uint32_t msb_right;
};

struct fb_var_screeninfo {
    uint32_t xres;
    uint32_t yres;
    uint32_t xoffset;
    uint32_t yoffset;
    uint32_t bits_per_pixel;
    struct fb_bitfield red;
    struct fb_bitfield green;
    struct fb_bitfield blue;
    struct fb_bitfield trans;
};

void fbdev_init(void);

#endif /* _AMETHYST_SYS_FB_H */

