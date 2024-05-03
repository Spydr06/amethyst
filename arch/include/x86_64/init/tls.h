#ifndef _AMETHYST_X86_64_BOOT_TLS_H
#define _AMETHYST_X86_64_BOOT_TLS_H

#include <stdint.h>

struct tls_table {
    void* tls_data;
    int cpuid;
    uintptr_t __pad[3];
    uintptr_t guard;
};

extern struct tls_table tls;

#endif /* _AMETHYST_X86_64_BOOT_TLS_H */

