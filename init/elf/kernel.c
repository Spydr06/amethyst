#include <limine.h>
#include <encoding/elf.h>

#include <kernelio.h>
#include <memory.h>
#include <string.h>
#include <assert.h>

static volatile bool kernel_elf_initialized = false;
static void *kernel_elf_addr;
static const Elf64_Shdr *kernel_strtab;

void kernel_elf_init(struct limine_kernel_file_response *response) {
    if(!__sync_bool_compare_and_swap(&kernel_elf_initialized, false, true))
        return;

    if(!response) {
        panic("Kernel file request failed.");
        return;
    }

    kernel_elf_addr = response->kernel_file->address;
    const Elf64_Ehdr *header = kernel_elf_header();
    if(memcmp(header->e_ident, ELFMAG, 4) || header->e_type != ET_EXEC) {
        panic( "Kernel file is not an ELF file.");
        return;
    }
}

bool kernel_elf_inited(void) {
    return kernel_elf_initialized;
}

const Elf64_Ehdr *kernel_elf_header(void) {
    assert(kernel_elf_initialized);
    return kernel_elf_addr;
}

const Elf64_Shdr *kernel_elf_section_header(Elf64_Half idx) {
    const Elf64_Ehdr *ehdr = kernel_elf_header();
    if(idx >= ehdr->e_shnum)
        return nullptr;

    return (const Elf64_Shdr*)((uintptr_t) ehdr + ehdr->e_shoff + ehdr->e_shentsize * idx);
}

uintptr_t kernel_elf_section(Elf64_Half idx) {
    const Elf64_Ehdr *ehdr = kernel_elf_header();
    const Elf64_Shdr *shdr = kernel_elf_section_header(idx);
    if(!shdr)
        return 0;

    return (uintptr_t) ehdr + shdr->sh_offset + shdr->sh_entsize * idx;
}

const char *kernel_elf_section_name(Elf64_Half idx) {
    const Elf64_Ehdr *ehdr = kernel_elf_header();
    const Elf64_Shdr *shstrtab = kernel_elf_section_header(ehdr->e_shstrndx);
    const Elf64_Shdr *shdr = kernel_elf_section_header(idx);

    if(!shstrtab || !shdr || shdr->sh_name >= shstrtab->sh_size)
        return nullptr;

    uintptr_t section = kernel_elf_section(ehdr->e_shstrndx);
    return (const char*)(section + shdr->sh_name);
}

const Elf64_Shdr *kernel_elf_find_section(const char *name) {
    const Elf64_Ehdr *ehdr = kernel_elf_header();
    for(Elf64_Half i = 0; i < ehdr->e_shnum; i++) {
        const char *other = kernel_elf_section_name(i);
        if(other && strcmp(name, other) == 0)
            return kernel_elf_section_header(i);
    }

    return nullptr;
}

const Elf64_Sym *kernel_resolve_symbol(const char *name) {
    const Elf64_Ehdr *ehdr = kernel_elf_header();
    const Elf64_Shdr *strtab = kernel_elf_find_section(".strtab");

    const Elf64_Shdr *shdr;
    for(Elf64_Half i = 0; (shdr = kernel_elf_section_header(i)); i++) {
        if(shdr->sh_type != SHT_SYMTAB)
            continue;

        uintptr_t section = kernel_elf_section(i);
        for(uintptr_t j = 0; j < shdr->sh_size / shdr->sh_entsize; j++) {
            const Elf64_Sym *sym = (const Elf64_Sym*)(section + shdr->sh_entsize * j); 
            if(sym->st_name >= strtab->sh_size)
                continue;

            const char *other = (const char*) ehdr + strtab->sh_offset + sym->st_name;
            if(strcmp(other, name) == 0)
                return sym;
        }
    }

    return nullptr;
}

static const Elf64_Shdr *get_strtab(void) {
    if(kernel_strtab)
        return kernel_strtab;
    return kernel_strtab = kernel_elf_find_section(".strtab");
}

const char *kernel_lookup_symbol(uintptr_t addr) {
    const Elf64_Ehdr *ehdr = kernel_elf_header();
    const Elf64_Shdr *strtab = get_strtab();

    const Elf64_Shdr *shdr;
    for(Elf64_Half i = 0; (shdr = kernel_elf_section_header(i)); i++) {
        if(shdr->sh_type != SHT_SYMTAB)
            continue;

        uintptr_t section = kernel_elf_section(i);
        for(uintptr_t j = 0; j < shdr->sh_size / shdr->sh_entsize; j++) {
            const Elf64_Sym *sym = (const Elf64_Sym*)(section + shdr->sh_entsize * j); 
            if(sym->st_name >= strtab->sh_size)
                continue;
            if(addr < sym->st_value || addr > sym->st_value + sym->st_size)
                continue;

            const char *other = (const char*) ehdr + strtab->sh_offset + sym->st_name;
            return other;
        }
    }

    return nullptr;
}
