#ifndef _AMETHYST_FB_H
#define _AMETHYST_FB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <bits/alltypes.h>

#define FB_TYPE_PACKED_PIXELS       0
#define FB_TYPE_PLANES              1
#define FB_TYPE_INTERLEAVED_PLANES  2
#define FB_TYPE_TEXT                3
#define FB_TYPE_VGA_PLANES          4

#define FB_VISUAL_TRUECOLOR         0

struct fb_fix_screeninfo {
    char id[16];
    
    void* smem_start;
    uint32_t smem_len;

    uint16_t type;
    uint16_t visual;

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

#ifdef __cplusplus
}
#endif

#endif /* _AMETHYST_FB_H */

