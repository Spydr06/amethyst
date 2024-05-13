#include <cpu/cpu.h>
#include <init/interrupts.h>
#include <drivers/video/console.h>
#include <drivers/video/vga.h>
#include <drivers/pci/pci.h>
#include <mem/heap.h>
#include <mem/vmm.h>
#include <mem/pmm.h>
#include <mem/mmap.h>
#include <sys/scheduler.h>
#include <x86_64/cpu/acpi.h>
#include <x86_64/cpu/gdt.h>
#include <x86_64/cpu/smp.h>
#include <x86_64/dev/pic.h>
#include <x86_64/dev/apic.h>
#include <x86_64/dev/cmos.h>
#include <x86_64/dev/hpet.h>
#include <x86_64/dev/pit.h>
#include <limine/limine.h>
#include <tty.h>

#include <kernelio.h>
#include <version.h>
#include <cdefs.h>
#include <stdint.h>
#include <assert.h>

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

static volatile struct limine_kernel_file_request kernel_file_request = {
    .id = LIMINE_KERNEL_FILE_REQUEST,
    .revision = 0
};

__noreturn void _start(void)
{
    cpu_set(&init_cpu);
    early_console_init();
     
    klog(DEBUG, "_start() is at %p", (void*) _start);

    gdt_reload();
    init_pit(SYSTEM_TICK_FREQUENCY);   
    pic_init();
    init_interrupts();
    
    struct mmap mmap;
    assert(mmap_parse(&mmap) == 0);
    pmm_init(&mmap);
    mmu_init(&mmap);
    vmm_init(&mmap);

    kernel_heap_init();

    int err = vga_init();
    if(err)
        klog(ERROR, "Failed initializing VGA driver: %s", strerror(err));
    else
        vga_console_init(VGACON_DEFAULT_OPTS);   

    acpi_init();
    hpet_init();

    cmos_init();

    struct tm tm;
    cmos_read(&tm);

    apic_init();
    scheduler_init();
    smp_init();

    if(kernel_file_request.response) {
        const char* cmdline = kernel_file_request.response->kernel_file->cmdline;
        kmain(strlen(cmdline), cmdline);
    }
    else
        kmain(0, nullptr);
    
    hlt();
}

