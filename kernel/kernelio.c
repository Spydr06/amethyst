#include "kernelio.h"
#include "math.h"
#include "string.h"
#include "sys/timekeeper.h"

#include <ctype.h>
#include <stdint.h>
#include <sys/tty.h>
#include <cpu/cpu.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/spinlock.h>

kernelio_writer_t kernelio_writer = early_putchar;

static char stdin_buffer[1024];
static size_t stdin_buffer_size = 0;

static spinlock_t io_lock = 0;

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

struct padding {
    char fill;
    char default_fill;
    char buf[20];
    uint8_t buf_size;
};

static void reset_padding(struct padding* pad) {
    pad->fill = pad->default_fill;
    if(pad->buf_size) {
        pad->buf_size = 0;
        memset(pad->buf, 0, sizeof(pad->buf));
    }
}

static int get_padding(struct padding* pad) {
    if(!pad->buf_size)
        return -1;
    
    int padding = atoi(pad->buf);
    reset_padding(pad);
    return padding;
}

static int print_padding(kernelio_writer_t writer, const char* s, int padding, char fill) {
    if(padding <= 0)
        return 0;
    
    padding -= strlen(s);

    int printed = 0;
    while(padding-- > 0) {
        writer(fill);
        printed++;
    }
    return printed;
}

int vfprintk(kernelio_writer_t writer, const char* restrict format, va_list ap) {
#define WRITES(s) while(*(s)) { \
        printed++;              \
        writer(*(s)++);         \
    }

    int printed = 0;
    enum : uint8_t { M_CHAR, M_SHORT, M_INT, M_LONG, M_LONG_LONG, M_SIZE_T, M_PRETTY_SIZE_T, M_INTMAX_T } mode;
    
    struct padding high_padding = {0, .default_fill = ' '};
    struct padding low_padding = {0, .default_fill = '0'};
    struct padding* selected_padding = &high_padding;

    char buf[100];

    for(size_t i = 0; format[i]; i++) {
        if(format[i] != '%') {
            printed++;
            writer(format[i]);
            continue;
        }
        
        mode = M_INT;

        reset_padding(&high_padding);
        reset_padding(&low_padding);

        char c;
    repeat:
        switch(c = format[++i]) {
        case 's': {
            const char* s = va_arg(ap, const char*);
            if(!s)
                s = "(null)";

            int low_pad = get_padding(&low_padding);
            int high_pad = get_padding(&high_padding);
            size_t len = low_pad >= 0 ? strnlen(s, (size_t) low_pad) : strlen(s);

            printed += print_padding(writer, "", high_pad - len, high_padding.fill); 

            while(len-- > 0 && *s) {
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
            printed += print_padding(writer, s, sizeof(void*) * 2, '0');
            WRITES(s);
        } break;
        case 'd':
        case 'i': {
            const char* s = mode == M_CHAR ? itoa((int64_t) (char) va_arg(ap, int), buf, 10)
                : mode == M_SHORT ? itoa((int64_t) (short) va_arg(ap, int), buf, 10)
                : mode == M_INT ? itoa((int64_t) va_arg(ap, int), buf, 10)
                : mode == M_LONG ? itoa((int64_t) va_arg(ap, long), buf, 10)
                : mode == M_LONG_LONG ? itoa((int64_t) va_arg(ap, long long), buf, 10)
                : mode == M_INTMAX_T ? utoa((uint64_t) va_arg(ap, uintmax_t), buf, 10)
                : mode == M_PRETTY_SIZE_T ? pretty_size_toa(va_arg(ap, size_t), buf)
                : itoa((uint64_t) va_arg(ap, size_t), buf, 10);
            printed += print_padding(writer, s, get_padding(&high_padding), high_padding.fill);
            WRITES(s);
        } break;
        case 'x': {
            const char* s = mode == M_CHAR ? utoa((uint64_t) (unsigned char) va_arg(ap, unsigned int), buf, 16)
                : mode == M_SHORT ? utoa((uint64_t) (unsigned short) va_arg(ap, unsigned int), buf, 16)
                : mode == M_INT ? utoa((uint64_t) va_arg(ap, unsigned int), buf, 16) 
                : mode == M_LONG ? utoa((uint64_t) va_arg(ap, unsigned long), buf, 16)
                : mode == M_LONG_LONG ? utoa((uint64_t) va_arg(ap, unsigned long long), buf, 16)
                : mode == M_INTMAX_T ? utoa((uint64_t) va_arg(ap, uintmax_t), buf, 16)
                : utoa((uint64_t) va_arg(ap, size_t), buf, 16);
            printed += print_padding(writer, s, get_padding(&high_padding), high_padding.fill);
            WRITES(s);
        } break;
        case 'u': {
            const char* s = mode == M_CHAR ? utoa((uint64_t) (unsigned char) va_arg(ap, unsigned int), buf, 10)
                : mode == M_SHORT ? utoa((uint64_t) (unsigned short) va_arg(ap, unsigned int), buf, 10)
                : mode == M_INT ? utoa((uint64_t) va_arg(ap, unsigned int), buf, 10) 
                : mode == M_LONG ? utoa((uint64_t) va_arg(ap, unsigned long), buf, 10)
                : mode == M_LONG_LONG ? utoa((uint64_t) va_arg(ap, unsigned long long), buf, 10)
                : mode == M_SIZE_T ? utoa((uint64_t) va_arg(ap, size_t), buf, 10)
                : mode == M_PRETTY_SIZE_T ? pretty_size_toa(va_arg(ap, size_t), buf)
                : utoa((uint64_t) va_arg(ap, uintmax_t), buf, 10);
            printed += print_padding(writer, s, get_padding(&high_padding), high_padding.fill);
            WRITES(s);
        } break;
        case 'f': {
            double d = va_arg(ap, double);
            int precision = 6;
            int low_pad = get_padding(&low_padding);
            if(low_pad >= 0)
                precision = low_pad;
            // TODO: high padding
            printed += fprintk(writer, "%ld.%lu", (long) d, (unsigned long) ((double) (d - (double) (long) d) * (double) pow10(precision)));
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
        // nonstandard
        case 'Z':
            mode = M_PRETTY_SIZE_T;
            goto repeat;
        case '%':
            writer('%');
            printed++;
            break;
        case '0':
            if(selected_padding->buf_size > 0)
                goto digit;

            /* fall through */
        case ' ':
            selected_padding->fill = c;
            goto repeat;
        case '.':
            selected_padding = &low_padding;
            goto repeat;
        default:
        digit:
            if(isdigit(c) && selected_padding->buf_size < __len(selected_padding->buf)) {
                selected_padding->buf[selected_padding->buf_size++] = c;
                goto repeat;
            }
            printed += 2;
            writer('%');
            writer(format[i]);
        } 
    }

    return printed;
#undef WRITES
}

static bool last_was_inline = false;

void __panic(const char* file, int line, const char* func, struct cpu_context* ctx, const char* error, ...) {
    spinlock_acquire(&io_lock);
    if(last_was_inline)
        kernelio_writer('\n');

    printk("\e[91mPANIC [%s:%d in %s()] ", file, line, func); 

    va_list ap;
    va_start(ap, error);
    vprintk(error, ap);
    va_end(ap);

    printk("\e[0m\n\n[stack trace]:\n");
    dump_stack();

    if(ctx) {
        printk("\e[0m\n\n[registers]:\n");
        dump_registers(ctx);
    }

    spinlock_release(&io_lock);
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

static void print_timestamp(const char* file) {
    struct timespec ts = timekeeper_time_from_boot();
    printk("[%5lu.%03lu] %s: ", ts.s, ts.ns / 1'000'000, file); 
}

static void __klog_impl(enum klog_severity severity, bool newline, const char* file, const char* format, va_list ap) {
    if(severity < klog_min_severity) 
        return;

    spinlock_acquire(&io_lock);

    if(last_was_inline) {
        kernelio_writer('\n');
        last_was_inline = false;
    }

    if(colors[severity])
        puts(colors[severity]);

    print_timestamp(file);

    vprintk(format, ap);

    if(colors[severity])
        puts("\e[0m");

    if(newline)
        kernelio_writer('\n');
    else
        last_was_inline = true;

    spinlock_release(&io_lock);
}

void __klog(enum klog_severity severity, const char *file, const char *format, ...) {
     va_list ap;
     va_start(ap, format);
     __klog_impl(severity, true, file, format, ap);
     va_end(ap);
}

void __klog_inl(enum klog_severity severity, const char* file, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    __klog_impl(severity, false, file, format, ap); 
    va_end(ap);
}

void stdin_push_char(char c) {
    if(stdin_buffer_size >= __len(stdin_buffer)) {
        klog(WARN, "Kernel stdin buffer is full; dropping character '%c'.", c);
        return;
    }

    stdin_buffer[stdin_buffer_size++] = c;
    kernelio_writer(c);
}

char kgetc(void) {
    // TODO: wait for keyboard input
    if(stdin_buffer_size) {
        char c = stdin_buffer[0];
        memmove(stdin_buffer, stdin_buffer + 1, stdin_buffer_size - 1);
        return c;
    }

    return '\0';
}

char* kgets(char* s, unsigned size) {
    size = MIN(size, stdin_buffer_size);
    
    memcpy(s, stdin_buffer, size);
    memmove(stdin_buffer, stdin_buffer + size, stdin_buffer_size - size);

    return s;
}

