#include <ff/elf.h>

#include <errno.h>
#include <kernelio.h>

int elf_load(struct vnode* node, void* base, void** entry, char** interpreter, Elf64_auxv_list_t* auxv) {
    if(node->type != V_TYPE_REGULAR)
        return EACCES;

    Elf64_Ehdr header;



    unimplemented();
    return 0;
}
