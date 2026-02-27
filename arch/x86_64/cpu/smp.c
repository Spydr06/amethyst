#include <x86_64/cpu/smp.h>
#include <x86_64/cpu/gdt.h>
#include <x86_64/cpu/idt.h>
#include <x86_64/cpu/cpu.h>
#include <x86_64/mem/mmu.h>

#include <drivers/acpi/acpi.h>
#include <drivers/acpi/apic.h>
#include <drivers/acpi/hpet.h>

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

size_t volatile smp_cpus_awake = 0;
static volatile size_t smp_cpus_total = 0;
static struct cpu* smp_cpus;

static volatile struct limine_smp_request smp_request = {
    .id = LIMINE_SMP_REQUEST,
    .revision = 0
};

struct cpu* smp_get_cpu(unsigned smp_id) {
    assert(smp_id < smp_cpus_awake);
    return &smp_cpus[smp_id];
}

static inline void sync_cpu_wakeup(void) {
    __atomic_add_fetch(&smp_cpus_awake, 1, __ATOMIC_SEQ_CST);
    while(__atomic_load_n(&smp_cpus_awake, __ATOMIC_SEQ_CST) < smp_cpus_total)
        pause();
    assert(smp_cpus_awake == smp_cpus_total);
}

static inline void cpu_hlt(struct cpu_context*, void*) {
    __atomic_sub_fetch(&smp_cpus_awake, 1, __ATOMIC_SEQ_CST);
    hlt();
    unreachable();
}

static __noreturn void cpu_wakeup(struct limine_smp_info* smp_info) {
    struct cpu *cpu = (struct cpu*) smp_info->extra_argument;
    memset(cpu, 0, sizeof(struct cpu));

    cpu_set(cpu);
    cpu_enable_features();

    gdt_reload();
    interrupts_apinit();

    interrupt_register(0xfd, cpu_hlt, NULL, IPL_IGNORE);

    mmu_apswitch();
    vmm_apinit();

    apic_initap();
    apic_timer_init();

    dpc_init();

    sync_cpu_wakeup();

    scheduler_apentry();

    // let the scheduler take over
    sched_stop_thread();  
}

void smp_init(void) {
    if(!smp_request.response) {
        klog(WARN, "SMP is not available");
        return;
    }

    smp_cpus_total = smp_request.response->cpu_count;
    klog(DEBUG, "[%zu] smp processor%s", smp_cpus_total, smp_cpus_total == 1 ? "" : "s");

    size_t smp_cpu_size = ROUND_UP(sizeof(struct cpu) * smp_cpus_total, PAGE_SIZE);

    smp_cpus = pmm_alloc(smp_cpu_size / PAGE_SIZE, PMM_SECTION_DEFAULT);
    assert(smp_cpus);
    smp_cpus = MAKE_HHDM(smp_cpus);

    memset(smp_cpus, 0, smp_cpu_size);

    for(size_t i = 0; i < smp_cpus_total; i++) {
        if(smp_request.response->cpus[i]->lapic_id == smp_request.response->bsp_lapic_id) {
            continue;
        }

        smp_request.response->cpus[i]->extra_argument = (uint64_t) &smp_cpus[i];

        __atomic_store_n(&smp_request.response->cpus[i]->goto_address, cpu_wakeup, __ATOMIC_SEQ_CST);
    }

    sync_cpu_wakeup();

    klog(DEBUG, "awoke other processors");
}

void smp_send_ipi(struct cpu* cpu, struct isr* isr, enum smp_ipi_target target, bool nmi) {
    apic_send_ipi(cpu ? cpu->id : 0, ISR_ID_TO_VECTOR(isr->id), target, nmi ? APIC_MODE_NMI : 0, 0);
}

__noreturn void smp_hlt(void) {
    if(smp_cpus_awake > 1) {
        smp_send_ipi(_cpu(), &_cpu()->isr[0xfd], SMP_IPI_OTHERCPUS, true);
    }

    hlt();
    unreachable();
}
