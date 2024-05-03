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
#include <x86_64/cpu/acpi.h>
#include <x86_64/dev/pic.h>
#include <x86_64/dev/pit.h>
#include <x86_64/cpu/gdt.h>
#include <drivers/pci/pci.h>
#include <mem/heap.h>
#include <mem/vmm.h>
#include <limine/limine.h>

extern void _start(void);
extern void kmain(size_t cmdline_size, const char* cmdline);

uint64_t __millis = 0;

uint64_t millis(void) {
    return __millis;
}

static struct cpu init_cpu;

uintptr_t __stack_chk_guard = _SSP;

__noreturn void __stack_chk_fail(void) {
    // TODO: maybe recover?
    panic("Stack smashing detected");
}

__noreturn void _start(void)
{
    cpu_set(&init_cpu);
    early_console_init();
 
    klog(DEBUG, "_start() is at %p", (void*) _start);

    gdt_reload();
    init_pit(SYSTEM_TICK_FREQUENCY);   
    pic_init();
    init_interrupts();
    
    int err = vga_init();
    if(err)
        klog(ERROR, "Failed initializing VGA driver: %s", strerror(err));
    else
        vga_console_init(VGACON_DEFAULT_OPTS);   

    pmm_init();


/*    if(!acpi_tag)
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
    
    hlt();
}

