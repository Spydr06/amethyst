#ifndef _AMETHYST_FF_PSF_H
#define _AMETHYST_FF_PSF_H

#include <stdint.h>

#define PSF1_FONT_MAGIC 0x0436
#define PSF2_FONT_MAGIC 0x864ab572

enum PSF_version : uint8_t {
    PSF_INVALID = 0,
    PSF_V1 = 1,
    PSF_V2 = 2
};

struct PSF1_header {
    uint16_t magic;
    uint8_t font_mode;
    uint8_t char_size;
} __attribute__((packed));

struct PSF2_header {
    uint32_t magic;
    uint32_t version;
    uint32_t header_size;
    uint32_t flags;
    uint32_t numglyph;
    uint32_t bytesperglyph;
    uint32_t height;
    uint32_t width;
} __attribute__((packed));

#ifndef NO_EMBED_FONT
extern uint8_t _binary_fonts_default_psf_start[];
extern uint8_t _binary_fonts_default_psf_end;
extern uint8_t _binary_fonts_default_psf_size;
#endif

enum PSF_version psf_get_version(uint8_t* font_start);
uint8_t* psf_get_glyph(uint32_t symbol, uint8_t* font_start);
uint32_t psf_get_width(uint8_t* font_start);
uint32_t psf_get_height(uint8_t* font_start);

#endif /* _AMETHYST_FF_PSF_H */

