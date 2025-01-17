#ifndef _AMETHYST_SYS_MMAP_H
#define _AMETHYST_SYS_MMAP_H

// flags for the `mmap(6)` syscall

#include <mem/vmm.h>

enum map_prot {
    PROT_NONE  = 0,
    PROT_READ  = 1,
    PROT_WRITE = 2,
    PROT_EXEC  = 4,

    _PROT_KNOWN = PROT_READ | PROT_WRITE | PROT_EXEC
};

enum map_flags {
    MAP_SHARED    = 0x01,
    MAP_PRIVATE   = 0x02,
    MAP_FIXED     = 0x10,
    MAP_ANONYMOUS = 0x20,

    _MAP_KNOWN = MAP_SHARED | MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS
};

static inline enum mmu_flags prot_to_mmu_flags(enum map_prot prot) {
    enum mmu_flags flags = 0;
    if(prot & PROT_READ)
        flags |= MMU_FLAGS_READ;
    if(prot & PROT_WRITE)
        flags |= MMU_FLAGS_WRITE;
    if((prot & PROT_EXEC) == 0)
        flags |= MMU_FLAGS_NOEXEC;
    return flags;
}

static inline enum vmm_flags mmap_to_vmm_flags(enum map_flags map_flags) {
    enum vmm_flags vmm_flags = 0;
    if(map_flags & MAP_SHARED)
        vmm_flags |= VMM_FLAGS_SHARED;
    if(map_flags & MAP_FIXED)
        vmm_flags |= VMM_FLAGS_EXACT;
    if((map_flags & MAP_ANONYMOUS) == 0)
        vmm_flags |= VMM_FLAGS_FILE;
    return vmm_flags;
}

#endif /* _AMETHYST_SYS_MMAP_H */

