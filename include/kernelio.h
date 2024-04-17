#ifndef _AMETHYST_KERNELIO_H
#define _AMETHYST_KERNELIO_H

#include <cdefs.h>
#include <stdarg.h>

#define panic(...) (__panic(__FILE__, __LINE__, __func__, __VA_ARGS__))
#define klog(sev, ...) (__klog(KLOG_##sev, __VA_ARGS__))

#define unimplemented() (panic("unimplemented()"))

typedef void (*kernelio_writer_t)(int);

enum klog_severity {
    KLOG_DEBUG = 1,
    KLOG_INFO,
    KLOG_WARN,
    KLOG_ERROR,

    __KLOG_MAX
};

extern kernelio_writer_t kernelio_writer;

int printk(const char* restrict format, ...) __attribute__((format(printf, 1, 2)));
int fprintk(kernelio_writer_t writer, const char* restrict format, ...) __attribute__((format(printf, 2, 3)));

int vprintk(const char* restrict format, va_list ap);
int vfprintk(kernelio_writer_t writer, const char* restrict format, va_list ap);

extern enum klog_severity klog_min_severity;
void __klog(enum klog_severity severity, const char* format, ...) __attribute__((format(printf, 2, 3)));

__noreturn void __panic(const char* file, int line, const char* function, const char* error, ...)
    __attribute__((format(printf, 4, 5)));

void dump_stack(void);

#endif /* _AMETHYST_KERNELIO_H */

