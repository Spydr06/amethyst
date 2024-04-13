#include <ff/psf.h>
#include <kernelio.h>
enum PSF_version psf_get_version(uint8_t* font_start) {
    struct PSF1_header* v1 = (struct PSF1_header*) font_start;
    if(v1->magic == PSF1_FONT_MAGIC)
        return PSF_V1;

    struct PSF2_header* v2 = (struct PSF2_header*) font_start;
    if(v2->magic == PSF2_FONT_MAGIC)
        return PSF_V2;
 
    panic("font not found");
    return PSF_INVALID;
}

uint8_t* psf_get_glyph(uint8_t symbol, uint8_t* font_start) {
    switch(psf_get_version(font_start)) {
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

uint32_t psf_get_width(uint8_t* font_start) {
    switch(psf_get_version(font_start)) {
        case PSF_V1:
            return 8;
        case PSF_V2:
            return ((struct PSF2_header*) font_start)->width;
        default:
            return 0;
    }
}

uint32_t psf_get_height(uint8_t* font_start) {
    switch(psf_get_version(font_start)) {
        case PSF_V1:
            return ((struct PSF1_header*) font_start)->char_size;
        case PSF_V2:
            return ((struct PSF2_header*) font_start)->height;
        default:
            return 0;
    }
}


