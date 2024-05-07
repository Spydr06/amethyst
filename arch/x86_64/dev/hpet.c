#include <x86_64/dev/hpet.h>
#include <x86_64/cpu/acpi.h>

#include <drivers/pci/pci.h>
#include <mem/vmm.h>

#include <time.h>

#include <kernelio.h>
#include <assert.h>

#define HPET_COUNTER_SIZE(hpet) (4 + (hpet)->counter_size * 4)

#define HPET_REG_CAPS 0
#define HPET_REG_CONFIG 2
#define HPET_REG_INTSTATUS 4
#define HPET_REG_COUNTER 0x1e

#define CAP_FSPERTICK(x) (((x) >> 32) & 0xfffffffful)

static struct HPET* hpet;
static time_t ticks_per_microsecond;

static __always_inline uintmax_t read(const struct HPET* hpet, unsigned reg) {
    switch(hpet->counter_size) {
    case 0:
        return ((uint32_t*) hpet->addr)[reg];
    case 1:
        return ((uint64_t*) hpet->addr)[reg];
    default:
        unreachable();
    }
}

static __always_inline void write(struct HPET* hpet, unsigned reg, uintmax_t value) {
    switch(hpet->counter_size) {
    case 0:
        ((uint32_t*) hpet->addr)[reg] = value;
        break;
    case 1:
        ((uint64_t*) hpet->addr)[reg] = value;
        break;
    default:
        unreachable();
    }
}

void hpet_init(void) {
    struct SDT_header* header = acpi_find_header(HPET_ACPI_HEADER_SIG);
    if(!header) {
        klog(WARN, "HPET not supported on this device.");
        return;
    }

    hpet = (struct HPET*) header;

    const struct pci_vendor_id* vendor = pci_lookup_vendor_id(hpet->pci_vendor_id);
    klog(INFO,
        "HPET (%hhu) by %s : revision %hhu : %hhu comparators (%u bits)",
        hpet->hpet_number,
        vendor ? vendor->vendor : "unknown",
        hpet->hardware_rev_id,
        hpet->comparator_count,
        HPET_COUNTER_SIZE(hpet) * 8
    );

    assert((hpet->addr & PAGE_SIZE) == 0);
    hpet->addr = (uint64_t) vmm_map(nullptr, PAGE_SIZE, VMM_FLAGS_PHYSICAL, MMU_FLAGS_READ | MMU_FLAGS_WRITE | MMU_FLAGS_NOEXEC, (void*) hpet->addr);
    assert(hpet->addr);

    ticks_per_microsecond = 1000000000 / CAP_FSPERTICK(read(hpet, HPET_REG_CAPS));
    klog(INFO, "%lu ticks per micro second (%lu fs per tick)", ticks_per_microsecond, CAP_FSPERTICK(read(hpet, HPET_REG_CAPS)));

    write(hpet, HPET_REG_CONFIG, 0);
    write(hpet, HPET_REG_COUNTER, 0);
    write(hpet, HPET_REG_CONFIG, 1);
}

