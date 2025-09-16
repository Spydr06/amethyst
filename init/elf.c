#include <ff/elf.h>

#include <mem/heap.h>
#include <mem/vmm.h>

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <kernelio.h>
#include <math.h>

#ifdef __x86_64__
    #include <x86_64/mem/mmu.h>
    
    #define _ELF_MACHINE EM_X86_64
#else
    #error "unsupported architecture"
#endif

#define STACK_TOP_BUFSIZ 8
#define STACK_MMU_FLAGS (MMU_FLAGS_READ | MMU_FLAGS_WRITE | MMU_FLAGS_NOEXEC | MMU_FLAGS_USER)
#define STACK_SIZE (1024 * 1024 * 4)

static int read_exact(struct vnode* node, void* buff, size_t count, uintmax_t offset) {
    size_t bytes_read;
    int err = vfs_read(node, buff, count, offset, &bytes_read, 0);
    if(err)
        return err;
    if(bytes_read != count)
        return ENOEXEC;
    return 0;
}

static bool validate_ehdr(Elf64_Ehdr* header) {
    return memcmp(header->e_ident, ELFMAG, 4) == 0 
        && header->e_type == ET_EXEC 
        && header->e_machine == _ELF_MACHINE;
}

static enum mmu_flags phdr_to_mmu_flags(Elf64_Word p_flags) {
    enum mmu_flags mmu_flags = MMU_FLAGS_USER;
    if(p_flags & PF_R)
        mmu_flags |= MMU_FLAGS_READ;
    if(p_flags & PF_W)
        mmu_flags |= MMU_FLAGS_WRITE;
    if((p_flags & PF_R) & PF_X)
        mmu_flags |= MMU_FLAGS_NOEXEC;

    return mmu_flags;
};

static int load(struct vnode* node, Elf64_Phdr* phdr, void** brk) {
    int err = 0;

    uintptr_t mem_address = phdr->p_vaddr;

    uintmax_t offset = phdr->p_offset;
    size_t file_size = phdr->p_filesz;
    size_t mem_size = phdr->p_memsz;

    enum mmu_flags mmu_flags = phdr_to_mmu_flags(phdr->p_flags) | MMU_FLAGS_USER | MMU_FLAGS_WRITE;
    uintmax_t first_page_offset = mem_address % PAGE_SIZE;

    // unaligned first page
    if(first_page_offset) {
        size_t first_page_count = MIN(file_size, PAGE_SIZE - first_page_offset);
        void* page = (void*) ROUND_DOWN(mem_address, PAGE_SIZE);

        if(!vmm_map(page, PAGE_SIZE, VMM_FLAGS_EXACT | VMM_FLAGS_ALLOCATE, mmu_flags, nullptr))
            return ENOMEM;

        if((err = read_exact(node, (void*) mem_address, first_page_count, offset)))
            return err;

        memset(page, 0, first_page_offset);
        mem_address += first_page_count;
        file_size -= first_page_count;
        mem_size -= first_page_count;
        offset += first_page_count;

        uintmax_t remaining = PAGE_SIZE - mem_address % PAGE_SIZE;
        if(remaining && mem_size) {
            size_t remaining_mem_size = MIN(remaining, mem_size);
            size_t page_diff = ROUND_UP(mem_address, PAGE_SIZE) - mem_address;
            if(remaining_mem_size > page_diff)
                remaining_mem_size = page_diff;

            memset((void*) mem_address, 0, remaining_mem_size);
            mem_size -= remaining_mem_size;
            mem_address += remaining_mem_size;
        }
    }

    // map middle of file
    size_t file_page_count = file_size / PAGE_SIZE;
    if(file_page_count) {
        struct vmm_file_desc desc = {
            .node = node,
            .offset = offset
        };

        if(!vmm_map((void*) mem_address, file_page_count, VMM_FLAGS_EXACT | VMM_FLAGS_FILE | VMM_FLAGS_PAGESIZE, mmu_flags, &desc))
            return ENOMEM;

        size_t byte_size = file_page_count * PAGE_SIZE;
        if((err = read_exact(node, (void*) mem_address, byte_size, offset)))
            return err;

        mem_address += byte_size;
        mem_size -= byte_size;
        file_size -= byte_size;
        offset += byte_size;
    }

    // map end of file
    size_t last_page_count = file_size % PAGE_SIZE;
    if(last_page_count) {
        if(!vmm_map((void*) mem_address, PAGE_SIZE, VMM_FLAGS_EXACT | VMM_FLAGS_ALLOCATE, mmu_flags, nullptr))
            return ENOMEM;

        if((err = read_exact(node, (void*) mem_address, last_page_count, offset)))
            return err;

        memset((void*)(mem_address + last_page_count), 0, PAGE_SIZE - last_page_count);
        mem_address += PAGE_SIZE;
        mem_size -= MIN(mem_size, PAGE_SIZE);
    }

    // zero parts if needed
    if(mem_size) {
        if(!vmm_map((void*) mem_address, mem_size, VMM_FLAGS_EXACT, mmu_flags, nullptr))
            return ENOMEM;
    }

    if(brk)
        *brk = (void*) MAX((uintptr_t) *brk, ROUND_UP(mem_address, PAGE_SIZE));

    return 0;
}

int elf_load(struct vnode* node, void* base, void** entry, char** interpreter, Elf64_auxv_list_t* auxv, void** brk) {
    if(node->type != V_TYPE_REGULAR)
        return EACCES;

    int err = 0;
    Elf64_Ehdr header;

    if((err = read_exact(node, &header, sizeof(Elf64_Ehdr), 0)))
        return err;

    if(!validate_ehdr(&header))
        return ENOEXEC;

    auxv->null.a_type = AT_NULL;
    auxv->phdr.a_type = AT_PHDR;
    auxv->phnum.a_type = AT_PHNUM;
    auxv->phent.a_type = AT_PHENT;
    auxv->entry.a_type = AT_ENTRY;

    auxv->phnum.a_un.a_val = header.e_phnum;
    auxv->phent.a_un.a_val = header.e_phentsize;
    auxv->entry.a_un.a_val = header.e_entry;

    *entry = (void*)(header.e_entry + (uintptr_t) base);

    assert(header.e_phentsize == sizeof(Elf64_Phdr));

    size_t phtable_size = header.e_phentsize * header.e_phnum;
    Elf64_Phdr* phdrs = kmalloc(phtable_size);
    if(!phdrs)
        return ENOMEM;

    if((err = read_exact(node, phdrs, phtable_size, header.e_phoff)))
        goto cleanup;

    Elf64_Phdr* interpreter_phdr = nullptr;

    for(size_t i = 0; i < header.e_phnum; i++) {
        phdrs[i].p_vaddr += (uintptr_t) base;

        klog(DEBUG, "phdr %zu: %d @ %p (%Zu)", i, phdrs[i].p_type, (void*) phdrs[i].p_vaddr, phdrs[i].p_memsz);
        
        switch(phdrs[i].p_type) {
            case PT_INTERP:
                interpreter_phdr = phdrs + i;
                break;
            case PT_PHDR:
                auxv->phdr.a_un.a_val = phdrs[i].p_vaddr;
                break;
            case PT_LOAD:
                if((err = load(node, phdrs + i, brk)))
                    goto cleanup;
                break;
            default:
                klog(WARN, "ignored ELF program header %zx (type %d)", i, phdrs[i].p_type);
                break;
        }
    }

    if(interpreter_phdr) {
        char* buff = kmalloc(interpreter_phdr->p_filesz);
        if(!buff) {
            err = ENOMEM;
            goto cleanup;
        }

        if((err = read_exact(node, buff, interpreter_phdr->p_filesz, interpreter_phdr->p_offset)))
            goto cleanup;

        *interpreter = buff;
    }

cleanup:
    kfree(phdrs);
    return err;
}

void* elf_prepare_stack(void* top, Elf64_auxv_list_t* auxv, char** argv, char** envp) {
    size_t argc, envc;
    size_t argv_size = 0, envp_size = 0;

    for(argc = 0; argv[argc]; argc++)
        argv_size += strlen(argv[argc]) + 1;

    for(envc = 0; envp[envc]; envc++)
        envp_size += strlen(envp[envc]) + 1;

    // align stack to 16 bytes
    int align = ((argc + 1) + (envc + 1) + 1) & 1 ? 8 : 0;

    // envp data, argv data, envp, argv, argc, buffer
    size_t initial_size = argv_size + envp_size + (argc + envc) * sizeof(char*) + sizeof(size_t) + sizeof(Elf64_auxv_list_t) + align + STACK_TOP_BUFSIZ;

    size_t initial_size_round = ROUND_UP(initial_size, PAGE_SIZE);
    void* initial_page_base = (void*)((uintptr_t) top - initial_size_round);

    if(!vmm_map(initial_page_base, initial_size_round, VMM_FLAGS_ALLOCATE | VMM_FLAGS_EXACT, STACK_MMU_FLAGS, nullptr))
        return nullptr;

    void* stack_base = (void*)((uintptr_t) top - STACK_SIZE);
    size_t unallocated_size = STACK_SIZE - initial_size_round;

    if(!vmm_map(stack_base, unallocated_size, VMM_FLAGS_EXACT, STACK_MMU_FLAGS, NULL))
        return NULL;

    top = (void*)((uintptr_t) top - STACK_TOP_BUFSIZ);

    char* arg_data_start = (char*) ((uintptr_t) top - argv_size);
    char* env_data_start = (char*) ((uintptr_t) arg_data_start - envp_size);
    Elf64_auxv_list_t* auxv_start = (Elf64_auxv_list_t*) env_data_start - 1;

    auxv_start = (Elf64_auxv_list_t*) (((uintptr_t) auxv_start & ~(uintptr_t) 0x0f) - align);

    char** env_start = (char**) auxv_start - envc - 1;
    char** arg_start = (char**) env_start - argc - 1;
    size_t* argc_ptr = (size_t*) arg_start - 1;

    memcpy(auxv_start, auxv, sizeof(Elf64_auxv_list_t));

    for(size_t i = 0; i < argc; i++) {
        strcpy(arg_data_start, argv[i]);
        *arg_start++ = arg_data_start;
        arg_data_start += strlen(argv[i]) + 1;
    }

    for(size_t i = 0; i < envc; i++) {
        strcpy(env_data_start, envp[i]);
        *env_start++ = env_data_start;
        env_data_start += strlen(envp[i]) + 1;
    }

    *arg_start = nullptr;
    *env_start = nullptr;
    *argc_ptr = argc;

    return argc_ptr;
}
