#include "kernelio.h"
#include "string.h"

#include <stdint.h>
#include <tty.h>

#include <stddef.h>

kernelio_writer_t kernelio_writer = putchar;

int printk(const char* restrict format, ...) {
    va_list ap;
    va_start(ap, format);
    int res = vfprintk(kernelio_writer, format, ap);
    va_end(ap);
    return res;
}

int fprintk(kernelio_writer_t writer, const char* restrict format, ...) {
    va_list ap;
    va_start(ap, format);
    int res = vfprintk(writer, format, ap);
    va_end(ap);
    return res;
}

int vprintk(const char* restrict format, va_list ap) {
    return vfprintk(kernelio_writer, format, ap);
}

int vfprintk(kernelio_writer_t writer, const char* restrict format, va_list ap) {
    int printed = 0;

    char buf[100];
    enum { M_CHAR, M_SHORT, M_INT, M_LONG, M_LONG_LONG, M_SIZE_T, M_INTMAX_T } mode;

    for(size_t i = 0; format[i]; i++) {
        if(format[i] != '%') {
            printed++;
            writer(format[i]);
            continue;
        }
        
        mode = M_INT;

    repeat:
        switch(format[++i]) {
        case 's': {
            const char* s = va_arg(ap, const char*);
            while(*s) {
                printed++;
                writer(*s++);
            }
        } break;
        case 'c': {
            printed++;
            writer(va_arg(ap, int));
        } break;
        case 'p': {
            printed += 2;
            writer('0');
            writer('x');
            const char* s = utoa((uint64_t) va_arg(ap, void*), buf, 16);
            while(*s) {
                printed++;
                writer(*s++);
            }
        } break;
        case 'd':
        case 'i': {
            const char* s = mode == M_CHAR ? itoa((int64_t) (char) va_arg(ap, int), buf, 10)
                : mode == M_SHORT ? itoa((int64_t) (short) va_arg(ap, int), buf, 10)
                : mode == M_INT ? itoa((int64_t) va_arg(ap, int), buf, 10)
                : mode == M_LONG ? itoa((int64_t) va_arg(ap, long), buf, 10)
                : mode == M_LONG_LONG ? itoa((int64_t) va_arg(ap, long long), buf, 10)
                : utoa((uint64_t) va_arg(ap, size_t), buf, 10);
            while(*s) {
                printed++;
                writer(*s++);
            }
        } break;
        case 'x': {
            const char* s = mode == M_CHAR ? itoa((uint64_t) (unsigned char) va_arg(ap, unsigned int), buf, 16)
                : mode == M_SHORT ? utoa((uint64_t) (unsigned short) va_arg(ap, unsigned int), buf, 16)
                : mode == M_INT ? utoa((uint64_t) va_arg(ap, unsigned int), buf, 16) 
                : mode == M_LONG ? utoa((uint64_t) va_arg(ap, unsigned long), buf, 16)
                : mode == M_LONG_LONG ? utoa((uint64_t) va_arg(ap, unsigned long long), buf, 16)
                : utoa((uint64_t) va_arg(ap, size_t), buf, 16);
            while(*s) {
                printed++;
                writer(*s++);
            }
        } break;
        case 'u': {
            const char* s = mode == M_CHAR ? itoa((uint64_t) (unsigned char) va_arg(ap, unsigned int), buf, 10)
                : mode == M_SHORT ? utoa((uint64_t) (unsigned short) va_arg(ap, unsigned int), buf, 10)
                : mode == M_INT ? utoa((uint64_t) va_arg(ap, unsigned int), buf, 10) 
                : mode == M_LONG ? utoa((uint64_t) va_arg(ap, unsigned long), buf, 10)
                : mode == M_LONG_LONG ? utoa((uint64_t) va_arg(ap, unsigned long long), buf, 10)
                : utoa((uint64_t) va_arg(ap, size_t), buf, 10);
            while(*s) {
                printed++;
                writer(*s++);
            }

        } break;
        case 'h':
            if(mode == M_SHORT)
                mode = M_CHAR;
            else
                mode = M_SHORT;
            goto repeat;
        case 'l':
            if(mode == M_LONG)
                mode = M_LONG_LONG;
            else
                mode = M_LONG;
            goto repeat;
        case 'q':
            mode = M_LONG_LONG;
            goto repeat;
        case 'z':
            mode = M_SIZE_T;
            goto repeat;
        case '%':
            writer('%');
            printed++;
            break;
        default:
            printed += 2;
            writer('%');
            writer(format[i]);
        } 
    }

    return printed;
}

