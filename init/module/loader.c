#include <encoding/elf.h>
#include <init/module.h>

#include <mem/vmm.h>

#include <errno.h>
#include <kernelio.h>
#include <memory.h>

int alloc_section(struct vnode *node, const Elf64_Shdr *shdr, uintptr_t *section) {
    enum mmu_flags mmu_flags =elf_shdr_to_mmu_flags(shdr->sh_flags) | MMU_FLAGS_WRITE;
    void *addr = vmm_map(nullptr, shdr->sh_size, VMM_FLAGS_ALLOCATE, mmu_flags, nullptr);
    if(!addr)
        return ENOMEM;

    int err;
    if((err = elf_read_exact(node, addr, shdr->sh_size, shdr->sh_offset)))
        goto failure;
    
    /* here();
    enum mmu_flags mmu_flags = elf_shdr_to_mmu_flags(shdr->sh_flags);
    if((err = vmm_change_mmu_flags(addr, shdr->sh_size, mmu_flags, 0)))
        goto failure; */

    *section = (uintptr_t) addr;
    return 0;
failure:
    vmm_unmap(addr, shdr->sh_size, 0);
    return err;
}

static int map_section(struct vnode *node, const Elf64_Shdr *shdr, uintptr_t *section) {
    // TODO: map from file directly
    return alloc_section(node, shdr, section);
}

int kmodule_load_section(struct kmodule_mapping *map, Elf64_Half sh_idx) {
    const Elf64_Shdr *shdr = map->shdrs + sh_idx;
    switch(shdr->sh_type) {
        case SHT_NULL:
            break;
        case SHT_STRTAB:
        case SHT_RELA:
        case SHT_SYMTAB:
            if(!shdr->sh_size)
                return 0;

            return alloc_section(map->vnode, shdr, map->sections + sh_idx);
        default:
            if(!shdr->sh_size || !(shdr->sh_flags & SHF_ALLOC))
                return 0;

            return map_section(map->vnode, shdr, map->sections + sh_idx);
    }

    return 0;
}

void kmodule_unload_section(struct kmodule_mapping *map, Elf64_Half sh_idx) {
    if(sh_idx >= map->ehdr.e_shoff || !map->sections || !map->sections[sh_idx])
        return;

    vmm_unmap((void*) map->sections[sh_idx], map->shdrs[sh_idx].sh_size, 0);
    map->sections[sh_idx] = 0;
}

