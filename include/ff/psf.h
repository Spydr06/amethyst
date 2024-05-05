#ifndef _AMETHYST_FF_PSF_H
#define _AMETHYST_FF_PSF_H

#include <stdint.h>
#include <cdefs.h>
#include <kernelio.h>

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

static __always_inline enum PSF_version psf_get_version(uint8_t* font_start) {
    struct PSF1_header* v1 = (struct PSF1_header*) font_start;
    if(v1->magic == PSF1_FONT_MAGIC)
        return PSF_V1;

    struct PSF2_header* v2 = (struct PSF2_header*) font_start;
    if(v2->magic == PSF2_FONT_MAGIC)
        return PSF_V2;
     
    panic("font not found");
    return PSF_INVALID;
}

static __always_inline uint8_t* psf_get_glyph(uint32_t symbol, uint8_t* font_start, enum PSF_version version) {
    switch(version) {
        case PSF_V1: {
            struct PSF1_header* font = (struct PSF1_header*) font_start;
            return font_start + sizeof(struct PSF1_header) + (symbol * font->char_size);
        }
        case PSF_V2: {
            struct PSF2_header* font = (struct PSF2_header*) font_start;
            return font_start + font->header_size + (symbol * font->bytesperglyph);
        }
        default:
            return 0;
    }
}

static __always_inline uint32_t psf_get_width(uint8_t* font_start) {
    switch(psf_get_version(font_start)) {
        case PSF_V1:
            return 8;
        case PSF_V2:
            return ((struct PSF2_header*) font_start)->width;
        default:
            return 0;
    }
}

static __always_inline uint32_t psf_get_height(uint8_t* font_start) {
    switch(psf_get_version(font_start)) {
        case PSF_V1:
            return ((struct PSF1_header*) font_start)->char_size;
        case PSF_V2:
            return ((struct PSF2_header*) font_start)->height;
        default:
            return 0;
    }
}

#endif /* _AMETHYST_FF_PSF_H */

