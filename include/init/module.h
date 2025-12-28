#ifndef _AMETHYST_INIT_MODULE_H
#define _AMETHYST_INIT_MODULE_H

#include "sys/spinlock.h"
#include <amethyst/module.h>

#include <encoding/elf.h>
#include <filesystem/vfs.h>
#include <hashtable.h>
#include <limine.h>

struct kmodule_mapping {
    volatile int refcount;
    struct vnode *vnode;

    Elf64_Ehdr ehdr;
    Elf64_Shdr *shdrs;
    uintptr_t *sections;

    char **args;
};

struct kmodule {
    spinlock_t lock;
    volatile bool initialized;

    const struct amethyst_module_spec *spec;
    struct kmodule_mapping *mapping;
};

void kmodule_reloc_init(struct limine_kernel_file_response *response);
void kmodule_mapping_delete(struct kmodule_mapping *map);

int kmodule_load(struct vnode *node, size_t argc, char **args, enum amethyst_module_flags flags);
void kmodule_unload(struct kmodule *kmod);

int kmodule_load_section(struct kmodule_mapping *map, Elf64_Half sh_idx);
int kmodule_reloc_section(struct kmodule_mapping *map, Elf64_Half sh_idx);
void kmodule_unload_section(struct kmodule_mapping *map, Elf64_Half sh_idx);

const struct kmodule *kmodule_query(const char *name);

static inline void kmodule_mapping_hold(struct kmodule_mapping *map) {
    __atomic_add_fetch(&map->refcount, 1, __ATOMIC_SEQ_CST);
}

static inline void kmodule_mapping_release(struct kmodule_mapping *map) {
    if(__atomic_sub_fetch(&map->refcount, 1, __ATOMIC_SEQ_CST) <= 0)
        kmodule_mapping_delete(map);
}

#endif /* _AMETHYST_INIT_MODULE_H */

