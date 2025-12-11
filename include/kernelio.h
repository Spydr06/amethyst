#ifndef _AMETHYST_KERNELIO_H
#define _AMETHYST_KERNELIO_H

#include <cdefs.h>
#include <stdarg.h>

#include <amethyst/amethyst.h>

#include <cpu/cpu.h>

typedef void (*kernelio_writer_t)(int);

extern kernelio_writer_t kernelio_writer;
extern enum klog_severity klog_min_severity;

int printk(const char* restrict format, ...) __attribute__((format(printf, 1, 2)));
int fprintk(kernelio_writer_t writer, const char* restrict format, ...) __attribute__((format(printf, 2, 3)));

int vprintk(const char* restrict format, va_list ap);
int vfprintk(kernelio_writer_t writer, const char* restrict format, va_list ap);

void __klog(enum klog_severity severity, const char* file, const char* format, ...) __attribute__((format(printf, 3, 4)));
void __vklog(enum klog_severity severity, const char *file, const char *format, va_list ap);
void __klog_inl(enum klog_severity severity, const char* file, const char* format, ...) __attribute__((format(printf, 3, 4)));
void __vklog_inl(enum klog_severity severity, const char* file, const char* format, va_list ap);

void klog_set_log_level(const char* level);

__noreturn void __panic(const char* file, int line, const char* function, struct cpu_context* ctx, const char* error, ...)
    __attribute__((format(printf, 5, 6)));

void stdin_push_char(char c);
char kgetc(void);
char* kgets(char* s, unsigned size);

void dump_stack(void);
void dump_registers(struct cpu_context* ctx);

void kernelio_lock(void);
void kernelio_unlock(void);

#endif /* _AMETHYST_KERNELIO_H */

