#ifndef _AMETHYST_KERNELIO_H
#define _AMETHYST_KERNELIO_H

#include <cdefs.h>
#include <stdarg.h>

#include <cpu/cpu.h>

#define panic_r(ctx, ...) (__panic(__FILE__, __LINE__, __func__, (ctx), __VA_ARGS__))
#define panic(...) panic_r(nullptr, __VA_ARGS__)

#define klog(sev, ...) (__klog(KLOG_##sev, __FILENAME__, __VA_ARGS__))
#define klog_inl(sev, ...) (__klog_inl(KLOG_##sev, __FILENAME__, __VA_ARGS__))

#define here() (__klog(KLOG_DEBUG, __FILENAME__, "\e[34m" __FILE__ ":%d:%s(): here\e[0m", __LINE__, __func__))

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
void __klog(enum klog_severity severity, const char* file, const char* format, ...) __attribute__((format(printf, 3, 4)));
void __klog_inl(enum klog_severity severity, const char* file, const char* format, ...) __attribute__((format(printf, 3, 4)));

void klog_set_log_level(const char* level);

__noreturn void __panic(const char* file, int line, const char* function, struct cpu_context* ctx, const char* error, ...)
    __attribute__((format(printf, 5, 6)));

void stdin_push_char(char c);
char kgetc(void);
char* kgets(char* s, unsigned size);

void dump_stack(void);
void dump_registers(struct cpu_context* ctx);

#endif /* _AMETHYST_KERNELIO_H */

