#ifndef _AMETHYST_X86_COMMON_CPU_RSDT_H
#define _AMETHYST_X86_COMMON_CPU_RSDT_H

#include "acpi.h"

struct RSDT {
    struct ACPI_SDT_header header;
    uint32_t tables[];
} __attribute__((packed));

struct XSDT {
    struct ACPI_SDT_header header;
    uint64_t tables[];
} __attribute__((packed));

#endif /* _AMETHYST_X86_COMMON_CPU_RSDT */

