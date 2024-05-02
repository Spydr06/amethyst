#include <stdint.h>
#include <tty.h>
#include <kernelio.h>
#include <cpu/cpu.h>
#include <version.h>
#include <cdefs.h>
#include <drivers/video/console.h>
#include <drivers/video/vga.h>
#include <init/interrupts.h>
#include <mem/pmm.h>
#include <mem/mmap.h>
#include <x86-common/cpu/acpi.h>
#include <x86-common/dev/pic.h>
#include <x86-common/dev/pit.h>
#include <drivers/pci/pci.h>
#include <mem/heap.h>
#include <mem/vmm.h>

extern void _start(void);
extern void kmain(size_t cmdline_size, const char* cmdline);

uint64_t __millis = 0;

uint64_t millis(void) {
    return __millis;
}


__noreturn void _start(void)
{
    early_console_init();
    init_pit(SYSTEM_TICK_FREQUENCY);   
    pic_init();
    init_interrupts();
    
   // end_of_mapped_memory = (uintptr_t) &end_of_mapped_memory;
    /*pmm_setup(multiboot_ptr, mbi_size);

    if(!acpi_tag)
        panic("No ACPI tag received from bootloader.");
    if(acpi_tag->type == MULTIBOOT_TAG_TYPE_ACPI_OLD)
        acpi_parse_sdt((uintptr_t) (((struct multiboot_tag_old_acpi*) acpi_tag) + 1), MULTIBOOT_TAG_TYPE_ACPI_OLD);
    else
        acpi_parse_sdt((uintptr_t) (((struct multiboot_tag_new_acpi*) acpi_tag) + 1), MULTIBOOT_TAG_TYPE_ACPI_NEW);

    if(!meminfo_tag)
        panic("No memory information given by the bootloader.");

    mmap_setup(meminfo_tag);
    hhdm_map_physical_memory();    
    vmm_init(VMM_LEVEL_SUPERVISOR, nullptr); */

    //if(cmdline_tag)
    //    kmain(cmdline_tag->size, cmdline_tag->string);
    //else
        kmain(0, nullptr);

while(1);
    hlt();
}

