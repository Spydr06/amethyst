#include <encoding/elf.h>
#include <init/module.h>

#include <errno.h>
#include <kernelio.h>

static int resolve_symbol(struct kmodule_mapping *map, uintptr_t *symval, Elf64_Half link, Elf64_Word idx) {
    if(link == SHN_UNDEF || idx == SHN_UNDEF)
        return 0;

    if(link >= map->ehdr.e_shnum) {
        klog(ERROR, "Unknown symtab section %u.", link);
        return EINVAL;
    }

    const Elf64_Shdr *symtab = map->shdrs + link;
    uintptr_t symbols = map->sections[link];

    size_t symtab_entries = symtab->sh_size / symtab->sh_entsize;
    if(idx >= symtab_entries) {
        klog(ERROR, "Symbol table entry %x out of bounds (max %zx).", idx, symtab_entries);
        return ERANGE;
    }

    const Elf64_Sym *symbol = ((void*) symbols) + idx * symtab->sh_entsize;
    switch(symbol->st_shndx) {
        case SHN_UNDEF: { // external symbol
            if(symtab->sh_link >= map->ehdr.e_shnum) {
                klog(ERROR, "Unknown string table section %u.", symtab->sh_link);
                return EINVAL;
            }

            const char *name = ((const char*) map->sections[symtab->sh_link]) + symbol->st_name;
            const Elf64_Sym *target = kernel_resolve_symbol(name);
            if(!target) {
                klog(ERROR, "Undefined Symbol '%s'.", name);
                return EINVAL;
            }

            // klog(INFO, "'%s' found at %p.", name, (void*) target->st_value);
            *symval = target->st_value;
        } break;
        case SHN_ABS: // absolute symbol
            // klog(INFO, "absolute symbol at %p.", symbol->st_value);
            *symval = symbol->st_value;
            break;
        default: { // internal symbol
            if(symbol->st_shndx >= map->ehdr.e_shnum) {
                klog(ERROR, "Unknown symbol section %u.", symbol->st_shndx);
                return EINVAL;
            }

            const Elf64_Shdr *target = map->shdrs + symbol->st_shndx;
            if(symbol->st_value >= target->sh_size) {
                klog(ERROR, "Symbol offset %zx out of bounds of section %u (max %zx).", symbol->st_value, symbol->st_shndx, target->sh_size);
                return ERANGE;
            }

            // klog(INFO, "internal symbol at %p (+%lx, section %u, idx %u).", (void*) map->sections[symbol->st_shndx] + symbol->st_value, symbol->st_value, symbol->st_shndx, idx);
            *symval = map->sections[symbol->st_shndx] + symbol->st_value;
        } break;
    }

    return 0;
}

static int apply_reloc(struct kmodule_mapping *map, const Elf64_Shdr *shdr, const Elf64_Rela *rel) {
    // klog(DEBUG, "rela [section: %u, offset: %p, type: %zu, addend: %ld]", shdr->sh_info, (void*) rel->r_offset, ELF64_R_TYPE(rel->r_info), rel->r_addend);

    if(shdr->sh_info >= map->ehdr.e_shnum) {
        klog(ERROR, "Unknown section %u.", shdr->sh_info);
        return EINVAL;
    }

    if(rel->r_offset >= map->shdrs[shdr->sh_info].sh_size) {
        klog(ERROR, "Reloc offset %zx out of bounds (max %zx).", rel->r_offset, map->shdrs[shdr->sh_info].sh_size);
        return EINVAL;
    }

    uintptr_t base_addr = map->sections[shdr->sh_info];
    uintptr_t offset = rel->r_offset;

    uintptr_t symval = 0;
    int err;
    if((err = resolve_symbol(map, &symval, shdr->sh_link, ELF64_R_SYM(rel->r_info)))) {
        klog(ERROR, "Could not resolve symbol %x (type %lu).", shdr->sh_link, ELF64_R_SYM(rel->r_info)); 
        return err;
    }

    switch(ELF64_R_TYPE(rel->r_info)) {
        case R_X86_64_NONE:
            // no relocation
            break;
        case R_X86_64_64: {
            // symbol + offset
            uint64_t *ref = (uint64_t*)(base_addr + offset);
            *ref = symval + *ref + rel->r_addend;
        } break;
        default:
            klog(ERROR, "Unsupported relocation type %zu.", ELF64_R_TYPE(rel->r_info));
            return EINVAL;
    }
    
    return 0;
}

int kmodule_reloc_section(struct kmodule_mapping *map, Elf64_Half sh_idx) {
    const Elf64_Shdr *shdr = map->shdrs + sh_idx;

    if(shdr->sh_type != SHT_RELA)
        return 0; // not a relocation section

    uintptr_t section = map->sections[sh_idx];
    for(size_t i = 0; i < shdr->sh_size / shdr->sh_entsize; i++) {
        const Elf64_Rel *rel = ((void*) section) + i * shdr->sh_entsize;

        int err;
        if(shdr->sh_entsize >= sizeof(Elf64_Rela)) {
            err = apply_reloc(map, shdr, (const Elf64_Rela*) rel);
        }
        else {
            const Elf64_Rela rela = {
                .r_offset = ((const Elf64_Rel*) rel)->r_offset,
                .r_info = ((const Elf64_Rel*) rel)->r_info,
                .r_addend = 0
            };
            err = apply_reloc(map, shdr, &rela);
        }
        if(err) {
            klog(ERROR, "Could not relocate symbol '%p' [%zx]", (void*) rel->r_offset, rel->r_info);
        }
    }

    return 0;
}
