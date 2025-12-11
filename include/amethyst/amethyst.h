#ifndef _AMETHYST_AMETHYST_H
#define _AMETHYST_AMETHYST_H

#define panic_r(ctx, ...) (__panic(__FILE__, __LINE__, __func__, (ctx), __VA_ARGS__))
#define panic(...) panic_r(nullptr, __VA_ARGS__)

#define klog(sev, ...) (__klog(KLOG_##sev, __FILENAME__, __VA_ARGS__))
#define klog_inl(sev, ...) (__klog_inl(KLOG_##sev, __FILENAME__, __VA_ARGS__))

#define vklog(sev, fmt, ap) (__vklog(KLOG_##sev, __FILENAME__, (fmt), (ap)))
#define vklog_inl(sev, fmt, ap) (__vklog_inl(KLOG_##sev, __FILENAME__, (fmt), (ap)))

#define here() (__klog(KLOG_DEBUG, __FILENAME__, "\e[34m" __FILE__ ":%d:%s(): here\e[0m", __LINE__, __func__))

#define unimplemented() (panic("unimplemented()"))

struct cpu_context;

enum klog_severity {
    KLOG_DEBUG = 1,
    KLOG_INFO,
    KLOG_WARN,
    KLOG_ERROR,

    __KLOG_MAX
};

int printk(const char* restrict format, ...) __attribute__((format(printf, 1, 2)));

void __klog(enum klog_severity severity, const char* file, const char* format, ...) __attribute__((format(printf, 3, 4)));
void __klog_inl(enum klog_severity severity, const char* file, const char* format, ...) __attribute__((format(printf, 3, 4)));

_Noreturn void __panic(const char* file, int line, const char* function, struct cpu_context* ctx, const char* error, ...)
    __attribute__((format(printf, 5, 6)));

#endif /* _AMETHYST_AMETHYST_H */

