#include <x86_64/trace.h>
#include <ff/elf.h>

#include <kernelio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct stackframe {
    struct stackframe* rbp; // ebp on x86
    uintptr_t rip; // eip on x86
};

static bool symtab_loaded = false;

static const uint8_t* strtab;
static const Elf64_Sym* symtab;
static const Elf64_Shdr* symtab_header;

void load_symtab(struct limine_kernel_file_response* response) {
    if(!response) {
        klog(ERROR, "kernel file request failed");
        return;
    }

    struct limine_file* kernel_file = response->kernel_file;

    const Elf64_Ehdr* header = kernel_file->address;

    if(memcmp(header->e_ident, ELFMAG, 4) || header->e_type != ET_EXEC) {
        klog(ERROR, "kernel file is not an ELF file");
        return;
    }

    const Elf64_Shdr* section_header_table = (Elf64_Shdr*) (((uint8_t*) header) + header->e_shoff);
    const Elf64_Shdr* strsect_header = section_header_table + header->e_shstrndx;

    const char* string_table = (const char*) (((uint8_t*) header) + strsect_header->sh_offset);

    for(size_t i = 0; i < header->e_shnum; i++) {
        const char* name = string_table + section_header_table[i].sh_name;
        
        if(strcmp(name, ".strtab") == 0)
            strtab = (uint8_t*) (((uint8_t*) header) + section_header_table[i].sh_offset);
        else if(strcmp(name, ".symtab") == 0) {
            symtab = (Elf64_Sym*) (((uint8_t*) header) + section_header_table[i].sh_offset);
            symtab_header = section_header_table + i;
        }
    }

    symtab_loaded = true;
}

const char* symtab_lookup(uintptr_t address) {
    if(!symtab_loaded)
        return nullptr;

    for(size_t i = 0; i < symtab_header->sh_size / sizeof(Elf64_Sym); i++) {
        if(address >= symtab[i].st_value && address < symtab[i].st_value + symtab[i].st_size)
            return (const char*) strtab + symtab[i].st_name;
    }

    return nullptr;
}


void __attribute__((no_sanitize("undefined"))) 
dump_stack(void) {
    struct stackframe* stack;
    stack = (struct stackframe*) __builtin_frame_address(0);

    for(size_t frame = 0; stack && frame < 4096; frame++) {
        if(stack->rip) {
            if(symtab_loaded) {
                const char* symbol = symtab_lookup(stack->rip);
                if(!symbol)
                    symbol = "...";
                printk("  0x%llu  [%s]\n", (unsigned long long) stack->rip, symbol);
            }
            else
                printk("  0x%llu\n", (unsigned long long) stack->rip);
        }
        stack = stack->rbp;
    }
}

void dump_registers(struct cpu_context* ctx) {
    printk("    rax = %lx, rbx = %lx, rcx = %lx, rdx = %lx\n", ctx->rax, ctx->rbx, ctx->rcx, ctx->rdx);
    printk("    r8 = %lx, r9 = %lx, r10 = %lx, r11 = %lx\n", ctx->r8, ctx->r9, ctx->r10, ctx->r11);
    printk("    r12 = %lx, r13 = %lx, r14 = %lx, r15 = %lx\n", ctx->r12, ctx->r13, ctx->r14, ctx->r15);
    printk("    gs = %lx, fs = %lx, es = %lx, ds = %lx\n", ctx->gs, ctx->fs, ctx->es, ctx->ds);
    printk("    cs = %lx, rflags = %lx,  ss = %lx\n", ctx->cs, ctx->rflags, ctx->ss);
    printk("    rip = %lx, rsp = %lx, rbp = %lx \n    error_code = %lx, cr2 = %lx\n", ctx->rip, ctx->rsp, ctx->rbp, ctx->error_code, ctx->cr2);
}
