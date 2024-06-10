#include <drivers/video/console.h>
#include <drivers/video/vga.h>

#include <mem/vmm.h>
#include <ff/psf.h>
#include <sys/tty.h>

#include <kernelio.h>
#include <math.h>
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TABSIZE 8

#define DEFAULT_FG_COLOR (vga_console.options & VGACON_COLORED ? vga_colors[7] : 0xffffffff)
#define DEFAULT_BG_COLOR (vga_console.options & VGACON_COLORED ? vga_colors[0] : 0x00000000)

enum cursor_movement : uint8_t {
    CURSOR_UP,
    CURSOR_DOWN,
    CURSOR_FORWARD,
    CURSOR_BACKWARD,
    CURSOR_LINE_DOWN,
    CURSOR_LINE_UP,
    CURSOR_HORIZONTAL_ABSOLUTE
};

struct vga_console vga_console = {0};

static uint32_t vga_colors[16] = {
    0x000000,
    0xd54e53,
    0xb9ca4a,
    0xe78c45,
    0x7aa6da,
    0xc397d8,
    0x70c0b1,
    0xe0e0e0,
    0x666666,
    0xff3334,
    0x9ec400,
    0xe7c547,
    0x7aa6da,
    0xb777e0,
    0x54c3d6,
    0xffffff
};

#define _GRID(x, y) ((vga_console.grid[((y) * vga_console.cols + (x) + vga_console.grid_offset) % vga_console.grid_size]))

void vga_console_init(uint8_t options) {
    if(!vga.address)
        panic("Called vga_console_init() without intitializing the vga driver prior.");

    vga_console.psf_font = _binary_fonts_default_psf_start;
    if((vga_console.psf_version = psf_get_version(vga_console.psf_font)) == PSF_INVALID)
        panic("Could not read builtin psf font at %p", vga_console.psf_font);

    vga_console.glyph_width = psf_get_width(vga_console.psf_font);
    vga_console.glyph_height = psf_get_height(vga_console.psf_font);

    vga_console.cols = vga.width / vga_console.glyph_width;
    vga_console.rows = vga.height / vga_console.glyph_height;
    vga_console.options = options;

    vga_console.cursor_x = 0;
    vga_console.cursor_y = 0;
    vga_console.cursor_mode = VGA_CURSOR_HIDDEN;

    vga_console.fg = DEFAULT_FG_COLOR;
    vga_console.bg = DEFAULT_BG_COLOR;

    vga_console.grid_offset = 0;
    vga_console.grid_size = vga_console.rows * vga_console.cols;
    vga_console.grid = vmm_map(
        nullptr,
        vga_console.grid_size * sizeof(struct vga_console_char),
        VMM_FLAGS_ALLOCATE,
        MMU_FLAGS_READ | MMU_FLAGS_WRITE | MMU_FLAGS_NOEXEC, 
        nullptr
    );
    
    assert(vga_console.grid);
    memset(vga_console.grid, 0, vga_console.grid_size * sizeof(struct vga_console_char));

    vga_clear(vga_console.bg);

    vga_console_register();
}

void vga_console_winsize(struct winsize* dest) {
    assert(dest);

    dest->ws_col = vga_console.cols;
    dest->ws_row = vga_console.rows;
    dest->ws_xpixel = vga.width;
    dest->ws_ypixel = vga.height;
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
            case 38:
                if(nums_len <= i + 1)
                    break;
                switch(nums[++i]) {
                    case 2: // 24-bit rgb value
                        if(nums_len <= i + 3)
                            break;
                        vga_console.fg = (nums[i + 1] & 0xff) << 16 | (nums[i + 2] & 0xff) << 8 | (nums[i + 3] & 0xff); 
                        i += 3;
                        break;
                    case 5: // 8-bit color lookup
                        if(nums_len <= i + 1)
                            break;
                        vga_console.fg = vga_color_map[(uint8_t) nums[i + 1]];
                        i++;
                        break;
                }
                break;
            case 39:
                vga_console.fg = DEFAULT_FG_COLOR;
                break;
            case 40 ... 47:
                if(vga_console.options & VGACON_COLORED)
                    vga_console.bg = vga_colors[n - 40];
                break;
            case 48:
                if(nums_len <= i + 1)
                    break;
                switch(nums[++i]) {
                    case 2: // 24-bit rgb value
                        if(nums_len <= i + 3)
                            break;
                        vga_console.bg = (nums[i + 1] & 0xff) << 16 | (nums[i + 2] & 0xff) << 8 | (nums[i + 3] & 0xff); 
                        i += 3;
                        break;
                    case 5: // 8-bit color lookup
                        if(nums_len <= i + 1)
                            break;
                        vga_console.bg = vga_color_map[(uint8_t) nums[i + 1]];
                        i++;
                        break;
                }
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

static void move_cursor_sequence(int* nums, unsigned nums_len, enum cursor_movement movement) {
    if(!nums_len) {
        nums_len = 1;
        *nums = 1;
    }

    for(unsigned i = 0; i < nums_len; i++) {
        switch(movement) {
        case CURSOR_UP:
            vga_console.cursor_y = MAX(vga_console.cursor_y - nums[i], 0);
            break;
        case CURSOR_DOWN:
            vga_console.cursor_y = MIN(vga_console.cursor_y + nums[i], vga_console.rows - 1);
            break;
        case CURSOR_BACKWARD:
            vga_console.cursor_x = MAX(vga_console.cursor_x - nums[i], 0);
            break;
        case CURSOR_FORWARD:
            vga_console.cursor_x = MIN(vga_console.cursor_x + nums[i], vga_console.cols - 1);
            break;
        case CURSOR_LINE_DOWN:
            vga_console.cursor_y = MIN(vga_console.cursor_y + nums[i], vga_console.cols - 1);
            vga_console.cursor_x = 0;
            break;
        case CURSOR_LINE_UP:
            vga_console.cursor_y = MAX(vga_console.cursor_y - nums[i], 0);
            vga_console.cursor_x = 0;
            break;
        case CURSOR_HORIZONTAL_ABSOLUTE:
            vga_console.cursor_x = MAX(MIN(nums[i], (int32_t) vga_console.cols - 1), 0);
            break;
        }
    }
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
        case 'A':
            move_cursor_sequence(nums, nums_len, CURSOR_UP);
            break;
        case 'B':
            move_cursor_sequence(nums, nums_len, CURSOR_DOWN);
            break;
        case 'C':
            move_cursor_sequence(nums, nums_len, CURSOR_FORWARD);
            break;
        case 'D':
            move_cursor_sequence(nums, nums_len, CURSOR_BACKWARD);
            break;
        case 'E':
            move_cursor_sequence(nums, nums_len, CURSOR_LINE_DOWN);
            break;
        case 'F':
            move_cursor_sequence(nums, nums_len, CURSOR_LINE_UP);
            break;
        case 'H':
            move_cursor_sequence(nums, nums_len, CURSOR_HORIZONTAL_ABSOLUTE);
            break;
        default:
            break;
    }

    nums_len = 0;
    vga_console.esc_status = VGA_NO_ESC_SEQ;
    return 1;
}

static inline void _fb_putchar(struct vga_console_char* ch, uint32_t cursor_x, uint32_t cursor_y) {
    uint32_t x, y, line;
    size_t start_offset = (cursor_y * vga_console.glyph_height * vga.pitch) + (cursor_x * vga_console.glyph_width * sizeof(uint32_t));
    size_t offset = start_offset;

    if(!isprint(ch->ch)) {
        for(y = 0; y < vga_console.glyph_height; y++) {
            line = offset;
            for(x = 0; x < vga_console.glyph_width; x++) {
                *((uint32_t*) (vga.address + line)) = ch->bg;
                line += vga.bpp >> 3;
            }

            offset += vga.pitch;
        }
    }
    else {
        uint8_t* glyph = psf_get_glyph(ch->ch, vga_console.psf_font, vga_console.psf_version);
        size_t bytesperline = (vga_console.glyph_width + 7) >> 3;

        for(y = 0; y < vga_console.glyph_height; y++) {
            line = offset;
            for(x = 0; x < vga_console.glyph_width; x++) {
                *((uint32_t*) (vga.address + line)) = glyph[x >> 3] & (0x80 >> (x & 7)) ? ch->fg : ch->bg;
                line += vga.bpp >> 3;
            }

            glyph += bytesperline;
            offset += vga.pitch;
        }
    }

    if(ch->modifiers & VGACON_MOD_UNDERLINED) {
        line = start_offset + vga.pitch * (vga_console.glyph_height - 3);
        for(x = 0; x < vga_console.glyph_width; x++) {
            *((uint32_t*) (vga.address + line)) = vga_console.fg;
            line += vga.bpp >> 3;
        }
    }

    if(ch->modifiers & VGACON_MOD_CROSSED) {
        line = start_offset + vga.pitch * (vga_console.glyph_height >> 1);
        for(x = 0; x < vga_console.glyph_width; x++) {
            *((uint32_t*) (vga.address + line)) = vga_console.fg;
            line += vga.bpp >> 3;
        }
    }
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

static __always_inline void vga_console_flush_row(uint32_t y) {
    for(uint32_t x = 0; x < vga_console.cols; x++) {
        _fb_putchar(&_GRID(x, y), x, y);
    }
}

static __always_inline void vga_console_flush_scroll(void) {
    struct vga_console_char* ch;

    for(uint32_t y = 0; y < vga_console.rows; y++) {
        for(uint32_t x = 0; x < vga_console.cols; x++) {
            ch = &_GRID(x, y);
            if(!y || memcmp(ch, &_GRID(x, y - 1), sizeof(struct vga_console_char)))
                _fb_putchar(ch, x, y);
        }
    }
}

static __always_inline void vga_console_put_next(struct vga_console_char* ch) {
    struct vga_console_char* existing = &_GRID(vga_console.cursor_x, vga_console.cursor_y); 
    if(memcmp(ch, existing, sizeof(struct vga_console_char)) != 0)
        _fb_putchar(ch, vga_console.cursor_x, vga_console.cursor_y);

    *existing = *ch;
    vga_console.cursor_x++;
}

void vga_console_flush(void) {
    for(uint32_t y = 0; y < vga_console.rows; y++) {
        vga_console_flush_row(y);
    }
}

void vga_console_disable_writer_propagation(void) {
    vga_console.writer_before = nullptr;
}

void vga_console_putchar(int c) {
    if(vga_console.writer_before)
        vga_console.writer_before(c);
    if(vga_console.esc_status && parse_escape_sequence(c))
        return;

    switch(c) {
        case '\n':
            vga_console.cursor_y++;
            vga_console.cursor_x = 0;
            break;
        case '\b':
            if(vga_console.cursor_x) {
                vga_console.cursor_x--;
                vga_console_put_next(&(struct vga_console_char){
                    .ch = '\0',
                    .fg = vga_console.fg,
                    .bg = vga_console.bg,
                    .modifiers = vga_console.modifiers,
                });
                vga_console.cursor_x--;
            }
            break;
        case '\t':
            if(vga_console.cursor_x + (vga_console.cursor_x + 1) % TABSIZE >= vga_console.cols) {
                vga_console.cursor_y++;
                vga_console.cursor_x = 0;
            }
            else {
                for(uint32_t i = 0; i < (vga_console.cursor_x + 1) % TABSIZE; i++) {
                    vga_console_put_next(&(struct vga_console_char) {
                        .ch = ' ',
                        .fg = vga_console.fg & 0x00ffffff,
                        .bg = vga_console.bg & 0x00ffffff,
                        .modifiers = vga_console.modifiers
                    });
                } 
            }
            break;
        case '\r':
            vga_console.cursor_x = 0;
            break;
        case '\e':
            vga_console.esc_status = VGA_ESC_SEQ_BEGIN_PARSE;
            return;
        default:
            vga_console_put_next(&(struct vga_console_char) {
                .ch = c,
                .fg = vga_console.fg & 0x00ffffff,
                .bg = vga_console.bg & 0x00ffffff,
                .modifiers = vga_console.modifiers
            });
            break;
    }

    if(vga_console.cursor_x >= vga_console.cols) {
        vga_console.cursor_x -= vga_console.cols;
        vga_console.cursor_y++;
    }

    if(vga_console.cursor_y >= vga_console.rows) {
        vga_console.cursor_y--;
        vga_console.grid_offset = (vga_console.grid_offset + vga_console.cols) % vga_console.grid_size;
        memset(&_GRID(0, vga_console.rows - 1), 0, vga_console.cols * sizeof(struct vga_console_char));
        vga_console_flush_scroll();
    }
}

size_t vga_console_ttywrite(void* __unused internal, const char* str, size_t count) {
    for(size_t i = 0; i < count; i++)
        vga_console_putchar(str[i]);
    return count;
}
