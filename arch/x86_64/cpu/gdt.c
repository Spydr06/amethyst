#include <x86_64/cpu/gdt.h>
#include <x86_64/cpu/cpu.h>

#include <string.h>

extern void _gdt_reload(volatile const struct gdtr* gdtr);

static gdte_t template[GDT_NUM_ENTRIES] = {
    0, // NULL 0x0
	0x00af9b000000ffff, // code64 0x8
	0x00af93000000ffff, // data64 0x10
	0x00aff3000000ffff, // udata64 0x18
	0x00affb000000ffff, // ucode64 0x20
	0x0020890000000000, // low ist 0x28
	0x0000000000000000  // high ist
};

void gdt_reload(void) {
    struct cpu* cpu = _cpu();

    memcpy(cpu->gdt, template, sizeof(template));
    uintptr_t ist_addr = (uintptr_t) &cpu->ist;

    cpu->gdt[5] |= ((ist_addr & 0xff000000) << 32) | ((ist_addr & 0xff0000) << 16) | ((ist_addr & 0xffff) << 16) | sizeof(struct ist);
    cpu->gdt[6] = (ist_addr >> 32) & 0xffffffff;

    struct gdtr gdtr = {
        .size = sizeof(template) - 1,
        .address = (uintptr_t) &cpu->gdt
    };
    
    _gdt_reload(&gdtr); 
}

