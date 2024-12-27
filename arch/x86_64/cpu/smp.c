#include <x86_64/cpu/smp.h>
#include <x86_64/cpu/gdt.h>
#include <x86_64/cpu/idt.h>
#include <x86_64/cpu/cpu.h>
#include <x86_64/cpu/acpi.h>
#include <x86_64/dev/apic.h>
#include <x86_64/dev/hpet.h>
#include <x86_64/mem/mmu.h>

#include <cpu/cpu.h>

#include <sys/scheduler.h>
#include <sys/dpc.h>

#include <mem/vmm.h>
#include <mem/pmm.h>

#include <stddef.h>
#include <math.h>
#include <assert.h>
#include <kernelio.h>

#include <limine/limine.h>

size_t smp_cpus_awake = 1;

static volatile struct limine_smp_request smp_request = {
    .id = LIMINE_SMP_REQUEST,
    .revision = 0
};

static __noreturn void cpu_wakeup(struct limine_smp_info* smp_info) {
    cpu_set((struct cpu*) smp_info->extra_argument);
    cpu_enable_features();

    gdt_reload();
    interrupts_apinit();

    dpc_init();

    mmu_apswitch();
    vmm_apinit();

    apic_initap();
    apic_timer_init();

    __atomic_add_fetch(&smp_cpus_awake, 1, __ATOMIC_SEQ_CST);

    scheduler_apentry();

    // let the scheduler take over
    sched_stop_thread();  
}

void smp_init(void) {
    size_t cpu_count = apic_lapic_count();
    assert(smp_request.response && smp_request.response->cpu_count == cpu_count);

    klog(DEBUG, "%zu processor%s found", cpu_count, cpu_count > 1 ? "s" : "");

    struct lapic_entry lapics[cpu_count];
    assert(apic_get_lapic_entries(lapics, cpu_count) == cpu_count);

    uint8_t bootstrap_id;
    __asm__ volatile(
        "mov $1, %%eax;"
        "cpuid;"
        "shrl $24, %%ebx;"
        : "=b"(bootstrap_id)
    );

    struct cpu* ap_cpus = pmm_alloc(ROUND_UP(sizeof(struct cpu) * cpu_count, PAGE_SIZE) / PAGE_SIZE, PMM_SECTION_DEFAULT);
    assert(ap_cpus);
    ap_cpus = MAKE_HHDM(ap_cpus);

    void (*wakeup_fn)(struct limine_smp_info*) = cpu_wakeup;

    for(size_t i = 0; i < cpu_count; i++) {
        if(lapics[i].lapicid == bootstrap_id)
            continue;

        smp_request.response->cpus[i]->extra_argument = (uint64_t)&ap_cpus[i];

        __atomic_store_n(&smp_request.response->cpus[i]->goto_address, wakeup_fn, __ATOMIC_SEQ_CST);
    }

    if(wakeup_fn == cpu_wakeup)
        while(__atomic_load_n(&smp_cpus_awake, __ATOMIC_SEQ_CST) != cpu_count)
            pause();

    klog(INFO, "awoke other processors");
}

void smp_send_ipi(struct cpu* cpu, struct isr* isr, enum smp_ipi_target target, bool nmi) {
    apic_send_ipi(cpu ? cpu->id : 0, ISR_ID_TO_VECTOR(isr->id), target, nmi ? APIC_MODE_NMI : 0, 0);
}

