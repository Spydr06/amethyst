#include "kernelio.h"
#include "math.h"
#include "string.h"
#include "sys/spinlock.h"

#include <ctype.h>
#include <stdint.h>
#include <tty.h>
#include <processor.h>
#include <stddef.h>
#include <stdlib.h>

kernelio_writer_t kernelio_writer = early_putchar;

static inline void puts(const char* str) {
    while(*str)
        kernelio_writer(*str++);
}

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
#define WRITES(s) while(*(s)) { \
        printed++;              \
        writer(*(s)++);         \
    }

#define PAD(s) if(pad_buf_size > 0 && (padding = atoi(pad_buf) - strlen((s))) > 0) {\
        while(padding-- > 0) {                                                      \
            writer(pad_fill);                                                       \
            printed++;                                                              \
        }                                                                           \
        pad_buf_size = 0;                                                           \
        memset(pad_buf, 0, sizeof pad_buf);                                         \
    }

    int printed = 0;

    char buf[100];
    enum { M_CHAR, M_SHORT, M_INT, M_LONG, M_LONG_LONG, M_SIZE_T, M_INTMAX_T } mode;
    char pad_fill;
    char pad_buf[20] = {0};
    unsigned pad_buf_size;
    int padding;

    for(size_t i = 0; format[i]; i++) {
        if(format[i] != '%') {
            printed++;
            writer(format[i]);
            continue;
        }
        
        mode = M_INT;
        pad_fill = ' ';
        if(pad_buf_size) {
            pad_buf_size = 0;
            memset(pad_buf, 0, sizeof pad_buf);
        }

        char c;
    repeat:
        switch(c = format[++i]) {
        case 's': {
            const char* s = va_arg(ap, const char*);
            if(!s)
                s = "(null)";
            PAD(s);
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
            int l = strlen(s);
            while(l++ < (int) sizeof(void*) * 2) {
                printed++;
                writer('0');
            }
            WRITES(s);
        } break;
        case 'd':
        case 'i': {
            const char* s = mode == M_CHAR ? itoa((int64_t) (char) va_arg(ap, int), buf, 10)
                : mode == M_SHORT ? itoa((int64_t) (short) va_arg(ap, int), buf, 10)
                : mode == M_INT ? itoa((int64_t) va_arg(ap, int), buf, 10)
                : mode == M_LONG ? itoa((int64_t) va_arg(ap, long), buf, 10)
                : mode == M_LONG_LONG ? itoa((int64_t) va_arg(ap, long long), buf, 10)
                : itoa((uint64_t) va_arg(ap, size_t), buf, 10);
            PAD(s);
            WRITES(s);
        } break;
        case 'x': {
            const char* s = mode == M_CHAR ? utoa((uint64_t) (unsigned char) va_arg(ap, unsigned int), buf, 16)
                : mode == M_SHORT ? utoa((uint64_t) (unsigned short) va_arg(ap, unsigned int), buf, 16)
                : mode == M_INT ? utoa((uint64_t) va_arg(ap, unsigned int), buf, 16) 
                : mode == M_LONG ? utoa((uint64_t) va_arg(ap, unsigned long), buf, 16)
                : mode == M_LONG_LONG ? utoa((uint64_t) va_arg(ap, unsigned long long), buf, 16)
                : utoa((uint64_t) va_arg(ap, size_t), buf, 16);
            PAD(s);
            WRITES(s);
        } break;
        case 'u': {
            const char* s = mode == M_CHAR ? utoa((uint64_t) (unsigned char) va_arg(ap, unsigned int), buf, 10)
                : mode == M_SHORT ? utoa((uint64_t) (unsigned short) va_arg(ap, unsigned int), buf, 10)
                : mode == M_INT ? utoa((uint64_t) va_arg(ap, unsigned int), buf, 10) 
                : mode == M_LONG ? utoa((uint64_t) va_arg(ap, unsigned long), buf, 10)
                : mode == M_LONG_LONG ? utoa((uint64_t) va_arg(ap, unsigned long long), buf, 10)
                : utoa((uint64_t) va_arg(ap, size_t), buf, 10);
            PAD(s);
            WRITES(s);
        } break;
        case 'f': {
            double d = va_arg(ap, double);
            int prec = 6;
            if(pad_buf_size > 0)
                prec = atoi(pad_buf);
            printed += fprintk(writer, "%ld.%lu", (long) d, (unsigned long) (d - (long) d) * (unsigned long) pow10(prec));
            break;
        }
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
        case '0':
        case ' ':
            pad_fill = c;
            goto repeat;
        default:
            if(isdigit(c) && pad_buf_size < __len(pad_buf)) {
                pad_buf[pad_buf_size++] = c;
                goto repeat;
            }
            printed += 2;
            writer('%');
            writer(format[i]);
        } 
    }

    return printed;

#undef WRITES
#undef PAD
}

void __panic(const char* file, int line, const char* func, const char* error, ...) {
    printk("\e[91mPANIC [%s:%d in %s()] ", file, line, func); 

    va_list ap;
    va_start(ap, error);
    vprintk(error, ap);
    va_end(ap);

    printk("\e[0m\n\n[stack trace]:\n");
    dump_stack();

    hlt();
}

#ifdef NDEBUG
enum klog_severity klog_min_severity = KLOG_INFO;
#else
enum klog_severity klog_min_severity = KLOG_DEBUG;
#endif

static const char* colors[__KLOG_MAX] = {
    [KLOG_DEBUG] = "\e[38m",
    [KLOG_INFO] = nullptr,
    [KLOG_WARN] = "\e[33m",
    [KLOG_ERROR] = "\e[31m"
};

void __klog(enum klog_severity severity, const char* format, ...) {
    static spinlock_t klog_spinlock = {false};

    spinlock_acquire(&klog_spinlock);

    if(severity < klog_min_severity) 
        return;

    if(colors[severity])
        puts(colors[severity]);

    uint64_t millis = 0; // TODO
    printk("[%8lu] ", millis);

    va_list ap;
    va_start(ap, format);
    vprintk(format, ap);
    va_end(ap);

    if(colors[severity])
        puts("\e[0m");

    kernelio_writer('\n'); 

    spinlock_release(&klog_spinlock);
}

