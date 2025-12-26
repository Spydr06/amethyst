#ifndef _AMETHYST_MODULE_H
#define _AMETHYST_MODULE_H

#include <stdint.h>

typedef int (*module_main_t)(int argc, const char **argv);
typedef void (*module_cleanup_t)(void);

enum amethyst_module_flags : uint32_t {
    MODULE_DEFAULT_FLAGS = 0
};

struct amethyst_module_spec {
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
    static __attribute__((section(".modinfo"), used)) struct amethyst_module_spec __spec_##__LINE__ \
        = { __VA_ARGS__ };

#endif /* _AMETHYST_MODULE_H */

