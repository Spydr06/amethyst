#ifndef _AMETHYST_MODULE_H
#define _AMETHYST_MODULE_H

#if defined(_AMETHYST_KERNEL_SRC) || defined(_AMETHYST_MODULE_SRC)

#include <stdint.h>
#include <cdefs.h>

#define AMETHYST_MODINFO_SECTION ".modinfo"
#define AMETHYST_MODINFO_MAGIC 0x8fa19753bc65cc91ull

#define AMETHYST_EXPORT_SECTION ".export"

enum amethyst_module_flags {
    AMETHYST_MODULE_DEFAULT_FLAGS    = 0,
    AMETHYST_MODULE_INIT_NONBLOCKING = 0x01
};

typedef int (*module_main_t)(int argc, const char **argv);
typedef void (*module_cleanup_t)(void);

struct amethyst_module_spec {
    uint64_t magic;

    const char *name;
    const char *license;
    const char *desc;
    const char *version;
    enum amethyst_module_flags flags;

    module_main_t main_func;
    module_cleanup_t cleanup_func;
};

#define _MODULE_INFO(_name, _license, _version, _desc) .name = (_name), \
    .license = (_license), \
    .desc = (_desc), \
    .version = (_version)

#define _MODULE_REGISTER(...) \
    static __attribute__((section(AMETHYST_MODINFO_SECTION), used)) struct amethyst_module_spec __spec_##__LINE__ \
        = { .magic = AMETHYST_MODINFO_MAGIC, __VA_ARGS__ };

struct amethyst_module_export {
    const char *name;
    uintptr_t addr;
};

#define _MODULE_EXPORT(_symbol) \
    static __attribute__((section(AMETHYST_EXPORT_SECTION), used)) struct amethyst_module_export __export_##__LINE__ \
        = { .name = __quote(_symbol), .addr = (uintptr_t)(_symbol) };

#endif /* _AMETHYST_KERNEL_SRC || _AMETHYST_MODULE_SRC */

#endif /* _AMETHYST_MODULE_H */

