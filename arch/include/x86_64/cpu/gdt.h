#ifndef _AMETHYST_X86_64_CPU_GDT_H
#define _AMETHYST_X86_64_CPU_GDT_H

#include <stdint.h>

#define GDT_NUM_ENTRIES 7

typedef uint64_t gdte_t;

struct gdtr {
    uint16_t size;
    uint64_t address;
} __attribute__((packed));

void gdt_reload(void);

#endif /* _AMETHYST_X86_64_CPU_GDT_H */

