#include "amethyst/amethyst.h"
#include <init/module.h>
#include <amethyst/module.h>

#include <encoding/elf.h>
#include <hashtable.h>
#include <sys/spinlock.h>
#include <mem/heap.h>

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <kernelio.h>

static spinlock_t module_table_lock;
static hashtable_t module_table;

void kmodule_init(void) {
    spinlock_init(module_table_lock);
    
    int err;
    if((err = hashtable_init(&module_table, KMODULE_MAX)))
        panic("Could not initialize kernel module system: %s", strerror(err));
}

int kmodule_load(struct vnode *node, size_t argc, char **args, enum amethyst_module_flags flags) {
    int err = 0;
    Elf64_Ehdr header;

    if((err = elf_read_exact(node, &header, sizeof(Elf64_Ehdr), 0)))
        return err;

    if(!elf_validate_ehdr(&header, ET_REL))
        return ENOEXEC;

    unimplemented();
/*    assert(header.e_phentsize == sizeof(Elf64_Phdr));


    size_t phtable_size = header.e_phentsize * header.e_phnum;
    Elf64_Phdr* phdrs = kmalloc(phtable_size);
    if(!phdrs)
        return ENOMEM;

    if((err = elf_read_exact(node, phdrs, phtable_size, header.e_phoff)))
        goto cleanup;

    for(Elf64_Half i = 0; i < header.e_phnum; i++) {
        klog(DEBUG, "ELF phdr %x (type %d)", i, phdrs[i].p_type);
        switch(phdrs[i].p_type) {
            case PT_INTERP:

                break;
            case PT_PHDR:

                break;

            case PT_LOAD:

                break;
            default:
                klog(WARN, "ignored ELF program header %x (type %d)", i, phdrs[i].p_type);
                break;
        }
    } */

cleanup:
    //kfree(phdrs);
    return err;
}

