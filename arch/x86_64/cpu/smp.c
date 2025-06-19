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

#include <assert.h>
#include <kernelio.h>
#include <math.h>
#include <memory.h>
#include <stddef.h>

#include <limine.h>

size_t smp_cpus_awake = 1;
static struct cpu* smp_cpus;

static volatile struct limine_smp_request smp_request = {
    .id = LIMINE_SMP_REQUEST,
    .revision = 0
};

struct cpu* smp_get_cpu(unsigned smp_id) {
    assert(smp_id < smp_cpus_awake);
    return &smp_cpus[smp_id];
}

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
    if(!smp_request.response) {
        klog(WARN, "SMP is not available");
        return;
    }

    size_t cpu_count = smp_request.response->cpu_count;
    klog(DEBUG, "[%zu] smp processor%s", cpu_count, cpu_count == 1 ? "" : "s");

    size_t smp_cpu_size = ROUND_UP(sizeof(struct cpu) * smp_request.response->cpu_count, PAGE_SIZE);

    smp_cpus = pmm_alloc(smp_cpu_size / PAGE_SIZE, PMM_SECTION_DEFAULT);
    assert(smp_cpus);
    smp_cpus = MAKE_HHDM(smp_cpus);

    memset(smp_cpus, 0, sizeof(smp_cpu_size));

    void (*wakeup_fn)(struct limine_smp_info*) = cpu_wakeup;

    for(size_t i = 0; i < cpu_count; i++) {
        if(smp_request.response->cpus[i]->lapic_id == smp_request.response->bsp_lapic_id) {
            continue;
        }

        smp_request.response->cpus[i]->extra_argument = (uint64_t) &smp_cpus[i];

        __atomic_store_n(&smp_request.response->cpus[i]->goto_address, wakeup_fn, __ATOMIC_SEQ_CST);
    }

    if(wakeup_fn == cpu_wakeup)
        while(__atomic_load_n(&smp_cpus_awake, __ATOMIC_SEQ_CST) != cpu_count)
            pause();

    klog(DEBUG, "awoke other processors");

/*    assert(smp_request.response);

    size_t lapic_count = apic_lapic_count();

    klog(DEBUG, "%zu lapics, %zu smp processors", lapic_count, smp_request.response->cpu_count);

    struct lapic_entry lapics[lapic_count];
    assert(apic_get_lapic_entries(lapics, lapic_count) == lapic_count);

    uint8_t bootstrap_id;
    __asm__ volatile(
        "mov $1, %%eax;"
        "cpuid;"
        "shrl $24, %%ebx;"
        : "=b"(bootstrap_id)
    );



    klog(DEBUG, "bootstrap id: %hhu", bootstrap_id);
    for(size_t i = 0; i < lapic_count; i++) {
        klog(DEBUG, "lapic[%zu].id = %hhu", i, lapics[i].lapicid);
    }

    for(size_t i = 1; i < smp_request.response->cpu_count; i++) {
//        klog(DEBUG, "SMP core %zu, lapic %zu", i, i * lapics_per_cpu);
//        if(lapics[i].lapicid == bootstrap_id)
//            continue;
//        here();

        smp_request.response->cpus[i]->extra_argument = (uint64_t)&smp_cpus[i];

        __atomic_store_n(&smp_request.response->cpus[i]->goto_address, wakeup_fn, __ATOMIC_SEQ_CST);
    }

    if(wakeup_fn == cpu_wakeup)
        while(__atomic_load_n(&smp_cpus_awake, __ATOMIC_SEQ_CST) != smp_request.response->cpu_count)
            pause();

    klog(INFO, "awoke other processors");*/
}

void smp_send_ipi(struct cpu* cpu, struct isr* isr, enum smp_ipi_target target, bool nmi) {
    apic_send_ipi(cpu ? cpu->id : 0, ISR_ID_TO_VECTOR(isr->id), target, nmi ? APIC_MODE_NMI : 0, 0);
}

