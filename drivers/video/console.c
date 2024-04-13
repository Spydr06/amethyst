#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <video/console.h>
#include <video/vga.h>
#include <ff/psf.h>
#include <tty.h>
#include <kernelio.h>

#define TABSIZE 4

#define DEFAULT_FG_COLOR (vga_console.options & VGACON_COLORED ? vga_colors[7] : 0xffffffff)
#define DEFAULT_BG_COLOR (vga_console.options & VGACON_COLORED ? vga_colors[0] : 0x00000000)

struct vga_console vga_console = {0};

static uint32_t vga_colors[16] = {
    0x000000,
    0xCD3131,
    0x0DBC79,
    0xE5E510,
    0x2472C8,
    0xBC3FBC,
    0x11A8CD,
    0xB0B0B0,
    0xE5E5E5,
    0xE74856,
    0x23D164,
    0xE6E600,
    0x3B8EEA,
    0xD670D6,
    0x29B8DB,
    0xffffff
};

void vga_console_init(uint8_t options) {
    if(!vga.address)
        panic("Called vga_console_init() without intitializing the vga driver prior.");

    vga_console.psf_font = _binary_fonts_default_psf_start;
    if(psf_get_version(vga_console.psf_font) == PSF_INVALID)
        panic("Could not read builtin psf font at %p", vga_console.psf_font);

    vga_console.glyph_width = psf_get_width(vga_console.psf_font);
    vga_console.glyph_height = psf_get_height(vga_console.psf_font);

    vga_console.width = vga.width / vga_console.glyph_width;
    vga_console.height = vga.height / vga_console.glyph_height;
    vga_console.options = options;

    vga_console.current_x = 0;
    vga_console.current_y = 0;
    vga_console.cursor_x = 0;
    vga_console.current_y = 0;
    vga_console.cursor_mode = VGA_CURSOR_HIDDEN;

    vga_console.fg = DEFAULT_FG_COLOR;
    vga_console.bg = DEFAULT_BG_COLOR;

    vga_clear(vga_console.bg);
    
    vga_console_register();
}

void vga_console_register(void) {
    vga_console.writer_before = kernelio_writer;
    kernelio_writer = vga_console_putchar;
}

void vga_console_unregister(void) {
    kernelio_writer = vga_console.writer_before;
}

static int parse_escape_sequence_type(char c) {
    switch(c) {
        case '[': // control sequence
            vga_console.esc_status = VGA_ESC_SEQ_CSI;
            return 1;
        // TODO: support others
        default:
            return 0;
    }
}

static void select_graphic_rendition(int* nums, unsigned nums_len) {
    if(nums_len == 0)
        return;

    for(unsigned i = 0; i < nums_len; i++) {
        int n = nums[i];
        switch(n) {
            case 0:
                vga_console.fg = DEFAULT_FG_COLOR;
                vga_console.bg = DEFAULT_BG_COLOR; 
                vga_console.modifiers = 0;
                break;
            case 1 ... 6:
                vga_console.modifiers |= (1 << (n - 1));
                break;
            case 27:
            case 7: {
                uint32_t tmp = vga_console.fg;
                vga_console.fg = vga_console.bg;
                vga_console.bg = tmp;
            } break;
            case 8:
                vga_console.modifiers |= VGACON_MOD_CONCEALED;
                break;
            case 9:
                vga_console.modifiers |= VGACON_MOD_CROSSED;
                break;
            case 22:
                vga_console.modifiers &= ~(VGACON_MOD_BOLD | VGACON_MOD_FAINT);
                break;
            case 23:
                vga_console.modifiers &= ~(VGACON_MOD_BOLD | VGACON_MOD_ITALIC);
                break;
            case 24:
                vga_console.modifiers &= ~VGACON_MOD_UNDERLINED;
                break;
            case 25:
                vga_console.modifiers &= ~(VGACON_MOD_SLOW_BLINK | VGACON_MOD_RAPID_BLINK);
                break;
            case 28:
                vga_console.modifiers &= ~VGACON_MOD_CONCEALED;
                break;
            case 29:
                vga_console.modifiers &= ~VGACON_MOD_CROSSED;
                break;
            case 30 ... 37:
                if(vga_console.options & VGACON_COLORED)
                    vga_console.fg = vga_colors[n - 30];
                break;
            case 38: // TODO
                break;
            case 39:
                vga_console.fg = DEFAULT_FG_COLOR;
                break;
            case 40 ... 47:
                if(vga_console.options & VGACON_COLORED)
                    vga_console.bg = vga_colors[n - 40];
                break;
            case 48: // TODO
                break;
            case 49:
                vga_console.bg = DEFAULT_BG_COLOR;
                break;
            case 90 ... 97:
                if(vga_console.options & VGACON_COLORED)
                    vga_console.fg = vga_colors[n - 90 + 8];
                break;
            case 100 ... 107:
                if(vga_console.options & VGACON_COLORED)
                    vga_console.bg = vga_colors[n - 100 + 8];
                break;
            default: // unknown modifier: do nothing
                break;
        }
    }

    return;
}

static int parse_control_sequence(char c) {
    static char buf[50] = {0};
    static unsigned buf_len = 0;

    static int nums[20] = {0};
    static unsigned nums_len = 0;

    if(isdigit(c)) {
        buf[buf_len++] = c;
        return 1;
    }

    if(buf_len) {
        nums[nums_len++] = atoi(buf);
        buf_len = 0;
        memset(buf, 0, sizeof(buf));
    }

    switch(c) {
        case ';':
            return 1;
        case 'm': // select graphic rendition
            select_graphic_rendition(nums, nums_len);
            break;
        default:
            break;
    }

    nums_len = 0;
    vga_console.esc_status = VGA_NO_ESC_SEQ;
    return 1;
}

static int parse_escape_sequence(char c) {
    switch(vga_console.esc_status) {
        case VGA_ESC_SEQ_BEGIN_PARSE:
            if(parse_escape_sequence_type(c))
                return 1;
            goto finish;
        case VGA_ESC_SEQ_CSI:
            if(parse_control_sequence(c))
                return 1;
            goto finish;
        default:
        finish:
            vga_console.esc_status = VGA_NO_ESC_SEQ;
            return 0;
    }
}

void vga_console_putchar(int c) {
    if(vga_console.esc_status && parse_escape_sequence(c))
        return;

    vga_console.writer_before(c);
    switch(c) {
        case '\n':
            vga_console.current_y++;
            vga_console.current_x = 0;
            break;
        case '\t':
            vga_console.current_x += (vga_console.current_x + 1) % TABSIZE;
           break;
        case '\r':
            vga_console.current_x = 0;
            break;
        case ' ':
            vga_console.current_x++;
            break;
        case '\e':
            vga_console.esc_status = VGA_ESC_SEQ_BEGIN_PARSE;
            break;
        default:
            vga_putchar(c, vga_console.current_x, vga_console.current_y, vga_console.fg, vga_console.bg, vga_console.glyph_width, vga_console.glyph_height);
            vga_console.current_x++;
            break;
    }

    if(vga_console.current_x >= vga_console.width) {
        vga_console.current_x -= vga_console.width;
        vga_console.current_y++;
    }

    if(vga_console.current_y >= vga_console.height) {
        memmove(
            vga.address,
            vga.address + (vga_console.glyph_height * vga.pitch),
            vga.memory_size - (vga_console.glyph_height * vga.pitch)
        );
        for(
            uint32_t* i = vga.address + vga.memory_size - vga_console.glyph_height * vga.pitch;
            (uintptr_t) i < (uintptr_t) vga.address + vga.memory_size; i++) {
            *i = vga_console.bg;
        }
        vga_console.current_y--;
    }
}


