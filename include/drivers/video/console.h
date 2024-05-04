#ifndef _AMETHYST_VIDEO_CONSOLE_H
#define _AMETHYST_VIDEO_CONSOLE_H

#include <kernelio.h>

#include <stdint.h>
#include <stddef.h>

#define VGACON_COLORED (1 << 0)
#define VGACON_DEFAULT_OPTS (VGACON_COLORED)

#define VGACON_MOD_BOLD        (1 << 0)
#define VGACON_MOD_FAINT       (1 << 1)
#define VGACON_MOD_ITALIC      (1 << 2)
#define VGACON_MOD_UNDERLINED  (1 << 3)
#define VGACON_MOD_SLOW_BLINK  (1 << 4)
#define VGACON_MOD_RAPID_BLINK (1 << 5)
#define VGACON_MOD_CONCEALED   (1 << 6)
#define VGACON_MOD_CROSSED     (1 << 7)

enum cursor_mode : uint8_t {
    VGA_CURSOR_HIDDEN = 0,
    VGA_CURSOR_SHOWN = 1
};

enum esc_seq_status : uint8_t {
    VGA_NO_ESC_SEQ = 0,
    VGA_ESC_SEQ_BEGIN_PARSE,
    VGA_ESC_SEQ_CSI
};

struct vga_console_char {
    uint32_t ch;
    uint32_t fg : 24;
    uint32_t bg : 24;
    uint8_t modifiers;
} __attribute__((packed));

struct vga_console {
    uint8_t* psf_font;
    uint8_t glyph_width;
    uint8_t glyph_height;

    uint8_t options;

    enum cursor_mode cursor_mode;
    uint16_t cursor_x;
    uint16_t cursor_y;

    uint16_t cols; // width in chars
    uint16_t rows; // height in chars
    
    uint32_t fg;
    uint32_t bg;

    size_t grid_offset;
    struct vga_console_char* grid;

    kernelio_writer_t writer_before;

    uint8_t modifiers;
    enum esc_seq_status esc_status;
};

void vga_console_init(uint8_t options);
void vga_console_register(void);
void vga_console_unregister(void);

void vga_console_putchar(int c);
void vga_console_flush(void);

#endif /* _AMETHYST_VIDEO_CONSOLE_H */

