#include <stdio.h>

#include <internal/format.h>

#define _GNU_SOURCE
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <sys/param.h>

#define _AMETHYST_SOURCE
#include <assert.h>

enum fmt_flags {
    FMT_ALT       = 0x01, // '#'
    FMT_ZERO_PAD  = 0x02, // '0'
    FMT_LEFT_ADJ  = 0x04, // '-'
    FMT_BLANK     = 0x08, // ' '
    FMT_PLUS      = 0x10, // '+'
};

enum length_modifier {
    LMOD_CHAR = 1,    // hh
    LMOD_SHORT,       // h
    LMOD_LONG,        // l, q
    LMOD_LONG_LONG,   // ll
    LMOD_LONG_DOUBLE, // L
    LMOD_INTMAX,      // j
    LMOD_SIZE,        // z, Z
    LMOD_PTRDIFF,     // t
};

struct printf_state {
    fmtprinter_t printer;
    void *userp;
    va_list ap;

    bool errored;
    size_t bytes_written;
};

struct conversion {
    enum fmt_flags flags;

    bool has_width;
    int width;

    bool has_precision;
    int precision;

    enum length_modifier length_modifier;
    char specifier;
};

static void _print(const char *restrict str, size_t size, struct printf_state *state) {
    ssize_t res = state->printer(state->userp, str, size);
    if(res < 0)
        state->errored = true;
    state->bytes_written += size;
}

static void _parse_flags(struct conversion *conv, const char *restrict *format) {
    conv->flags = 0;

    for(;;) {
        switch(**format) {
            case '#':
                conv->flags |= FMT_ALT;
                break;
            case '0':
                conv->flags |= FMT_ZERO_PAD;
                break;
            case '-':
                conv->flags |= FMT_LEFT_ADJ;
                break;
            case ' ':
                conv->flags |= FMT_BLANK;
                break;
            case '+':
                conv->flags |= FMT_PLUS;
                break;
            default:
                return;
        }

        (*format)++;
    }
}

static int _parse_int(const char *restrict *format) {
    int i = 0;
    bool negate = **format == '-';

    if(negate)
        (*format)++;

    for(; isdigit(**format); (*format)++)
        i = i * 10 + **format - '0';

    return negate ? -i : i;
}

static void _parse_width(struct conversion *conv, const char *restrict *format, struct printf_state *state) {
    if(**format == '*') {
        (*format)++;
        conv->has_width = true;
        conv->width = va_arg(state->ap, int);
    }
    else if(isdigit(**format) || **format  == '-') {
        conv->has_width = true;
        conv->width = _parse_int(format);
    }
    else
        conv->has_width = false;
}

static void _parse_precision(struct conversion *conv, const char *restrict *format, struct printf_state *state) {
    conv->has_precision = **format == '.';

    if(!conv->has_precision)
        return;

    (*format)++;

    if(**format == '*') {
        (*format)++;
        conv->precision = va_arg(state->ap, int);
    }

    conv->precision = _parse_int(format);
}

static void _parse_length_modifier(struct conversion *conv, const char *restrict *format) {
    conv->length_modifier = 0;
    switch(**format) {
        case 'h':
            if((*format)[1] == 'h') {
                (*format)++;
                conv->length_modifier = LMOD_CHAR;
            }
            else
                conv->length_modifier = LMOD_SHORT;
            break;
        case 'l':
            if((*format)[1] == 'l') {
                (*format)++;
                conv->length_modifier = LMOD_LONG_LONG;
            }
            else
                conv->length_modifier = LMOD_LONG;
            break;
        case 'q':
            conv->length_modifier = LMOD_LONG_LONG;
            break;
        case 'L':
            conv->length_modifier = LMOD_LONG_DOUBLE;
            break;
        case 'j':
            conv->length_modifier = LMOD_INTMAX;
            break;
        case 'z':
        case 'Z':
            conv->length_modifier = LMOD_SIZE;
            break;
        case 't':
            conv->length_modifier = LMOD_PTRDIFF;
            break;
        default:
            return;
    }

    (*format)++;
}

static char* _reverse(char* str, size_t len) {
    char tmp;
    for(size_t i = 0; i < len / 2; i++) {
        tmp = str[i];
        str[i] = str[len - i - 1];
        str[len - i - 1] = tmp;
    }
    return str;
}

static void _utoa(char* buf, uintmax_t u, int base, bool upper) {
    size_t i = 0;
    if(u == 0) {
        buf[i++] = '0';
        buf[i] = '\0';
        return;
    }

    while(u != 0) {
        uint64_t rem = u % base;
        if(rem > 9)
            buf[i++] = ((rem - 10) + (upper ? 'A' : 'a'));
        else
            buf[i++] = rem + '0';
        u /= base;
    }

    buf[i] = '\0';
    _reverse(buf, i);
}

static void _int_conversion(struct conversion *conv, struct printf_state *state) {
    if(!conv->has_precision || conv->precision < 0)
        conv->precision = 1;

    char buf[65];

    intmax_t i;
    switch(conv->length_modifier) {
        case LMOD_CHAR:
            i = (volatile char) va_arg(state->ap, int);
            break;
        case LMOD_SHORT:
            i = (volatile short) va_arg(state->ap, int);
            break;
        case LMOD_LONG:
            i = va_arg(state->ap, long);
            break;
        case LMOD_LONG_LONG:
            i = va_arg(state->ap, long long);
            break;
        case LMOD_INTMAX:
            i = va_arg(state->ap, intmax_t);
            break;
        case LMOD_SIZE:
            i = va_arg(state->ap, ssize_t);
            break;
        case LMOD_PTRDIFF:
            i = va_arg(state->ap, ptrdiff_t);
            break;
        default:
            i = va_arg(state->ap, int);
            break;
    }

    if(i == 0 && conv->precision == 0)
        return;

    buf[0] = i < 0 ? '-' : conv->flags & FMT_PLUS ? '+' : conv->flags & FMT_BLANK ? ' ' : '\0';

    _utoa(buf[0] ? buf + 1 : buf, ABS(i), 10, false);

    size_t buf_len = strnlen(buf, sizeof(buf));
    char pad_char = conv->flags & FMT_ZERO_PAD && !(conv->flags & FMT_LEFT_ADJ) ? '0' : ' ';
    int pad = conv->precision - (int) buf_len;

    if(pad > 0) {
        char pad_buf[pad];
        memset(pad_buf, pad_char, pad);
        _print(pad_buf, pad, state);
    }

    _print(buf, buf_len, state);
}

static void _unsigned_conversion(struct conversion *conv, struct printf_state *state) {
    if(!conv->has_precision || conv->precision < 0)
        conv->precision = 1;

    char buf[65];
    uintmax_t u;

    buf[0] = '\0';

    switch(conv->length_modifier) {
        case LMOD_CHAR:
            u = (volatile unsigned char) va_arg(state->ap, unsigned);
            break;
        case LMOD_SHORT:
            u = (volatile unsigned short) va_arg(state->ap, unsigned);
            break;
        case LMOD_LONG:
            u = va_arg(state->ap, unsigned long);
            break;
        case LMOD_LONG_LONG:
            u = va_arg(state->ap, unsigned long long);
            break;
        case LMOD_INTMAX:
            u = va_arg(state->ap, uintmax_t);
            break;
        case LMOD_SIZE:
            u = va_arg(state->ap, size_t);
            break;
        case LMOD_PTRDIFF:
            u = va_arg(state->ap, uintptr_t);
            break;
        default:
            u = va_arg(state->ap, unsigned);
            break;
    }

    if(u == 0 && conv->precision == 0)
        return;

    bool zero_pad = conv->flags & FMT_ZERO_PAD && !(conv->flags & FMT_LEFT_ADJ);

    int base;
    switch(conv->specifier) {
        case 'x':
        case 'X':
            base = 16;
            if(conv->flags & FMT_ALT) {
                if(zero_pad)
                    _print((char[]){'0', conv->specifier}, 2, state);
                else {
                    buf[0] = '0';
                    buf[1] = conv->specifier;
                    buf[2] = '\0';
                }
            }
            break;
        case 'o':
            base = 8;
            if(conv->flags & FMT_ALT) {
                if(zero_pad)
                    _print("0", 1, state);
                else {
                    buf[0] = '0';
                    buf[1] = '\0';
                }
            }            
            break;
        default:
            base = 10;
            break;
    }

    _utoa(buf + strlen(buf), u, base, isupper(conv->specifier));

    size_t buf_len = strnlen(buf, sizeof(buf));
    char pad_char = zero_pad ? '0' : ' ';
    int pad = conv->precision - (int) buf_len;

    if(pad > 0) {
        char pad_buf[pad];
        memset(pad_buf, pad_char, pad);
        _print(pad_buf, pad, state);
    }

    _print(buf, buf_len, state);
}

static void _double_conversion(struct conversion *conv, struct printf_state *state) {
    (void) conv;
    (void) state;
    unimplemented();
}

static void _string_conversion(struct conversion *conv, struct printf_state *state, const void *str, size_t size) {
    if(conv->has_precision && conv->precision >= 0)
        size = MIN(size, (size_t) conv->precision);

    if(!size)
        return;

    int pad = conv->width - (int) size;
    if(conv->has_width && !(conv->flags & FMT_LEFT_ADJ) && pad > 0) {
        char pad_buf[pad];
        memset(pad_buf, ' ', pad);
        _print(pad_buf, pad, state);
    }

    _print(str, size, state);

    if(conv->has_width && conv->flags & FMT_LEFT_ADJ && pad > 0) {
        char pad_buf[pad];
        memset(pad_buf, ' ', pad);
        _print(pad_buf, pad, state);
    }
}

static void _format(const char *restrict *format, struct printf_state *state) {
    struct conversion conv;

    _parse_flags(&conv, format);
    _parse_width(&conv, format, state);
    _parse_precision(&conv, format, state);
    _parse_length_modifier(&conv, format);

    conv.specifier = (*format)++[0];

    switch(conv.specifier) {
        case 'd':
        case 'i':
            _int_conversion(&conv, state);
            break;
        case 'o':
        case 'u':
        case 'x':
        case 'X':
            _unsigned_conversion(&conv, state);
            break;
        case 'e':
        case 'E':
        case 'f':
        case 'F':
        case 'g':
        case 'G':
        case 'a':
        case 'A':
            _double_conversion(&conv, state);
            break;
        case 'c':
            _string_conversion(&conv, state, &conv.specifier, 1);
            break;
        case 's': {
                const char *s = va_arg(state->ap, const char*);
                size_t l = conv.has_precision && conv.precision >= 0 ? strnlen(s, conv.precision) : strlen(s);
                _string_conversion(&conv, state, s, l);
            } break;
        case 'p':
            conv.flags |= FMT_ALT | FMT_ZERO_PAD;
            conv.length_modifier = LMOD_LONG;
            conv.specifier = 'x';
            conv.has_precision = true;
            conv.precision = sizeof(void*) * 2;
            _unsigned_conversion(&conv, state);
            break;
        case 'n': {
                int *d = va_arg(state->ap, int*);
                *d = state->bytes_written; 
            } break;
        case 'm': {
                const char *e = strerror(errno);
                _string_conversion(&conv, state, e, strlen(e));
            } break;
        case FMT_CHAR:
            _print(&conv.specifier, 1, state);
            break;
    }
}

int printf_impl(fmtprinter_t printer, void *userp, const char *restrict format, va_list ap) {
    struct printf_state state = {0};
    state.printer = printer;
    state.userp = userp;
    va_copy(state.ap, ap);

    while(*format && !state.errored) {
        char* escape = strchrnul(format, FMT_CHAR);
        _print(format, escape - format, &state);
        format += escape - format;
        
        // end of format
        if(!*format)
            break;

        // parse format
        format++;
        _format(&format, &state);
    }

    return state.errored ? -1 : (int) state.bytes_written;
}
