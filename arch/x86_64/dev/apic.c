#include <x86_64/dev/apic.h>
#include <x86_64/cpu/acpi.h>

#include <mem/heap.h>
#include <mem/vmm.h>

#include <stddef.h>
#include <kernelio.h>
#include <assert.h>

static struct MADT* madt;
static struct apic_list_header *list_start, *list_end; 

static size_t override_count, io_count, lapic_count, lapic_nmi_count;

void *lapic_addr, *lapic_phys_addr;
static struct io_apic_desc* io_apics;

static __always_inline struct apic_list_header* next_entry(struct apic_list_header* entry) {
    return (struct apic_list_header*) (((uintptr_t) entry) + entry->length);
}

static size_t entry_count(enum MADT_entry_type type) {
    struct apic_list_header* entry = list_start;
    size_t count = 0;

    while(entry < list_end) {
        if(entry->type == type)
            count++;
        entry = next_entry(entry);
    }

    return count;
}

static struct apic_list_header* get_entry(enum MADT_entry_type type, intptr_t nth) {
    struct apic_list_header* entry = list_start;
    while(entry < list_end) {
        if(entry->type == type && nth-- == 0)
            return entry;
        entry = next_entry(entry);
    }

    return nullptr;
}

static uint32_t ioapic_read(void* ioapic, enum io_apic_register reg) {
    volatile uint32_t* apic = ioapic;
    *apic = (uint32_t) reg;
    return *(apic + 4);
}

static void ioapic_write(void* ioapic, enum io_apic_register reg, uint32_t value) {
    volatile uint32_t* apic = ioapic;
    *apic = (uint32_t) reg;
    *(apic + 4) = value;
}

static void iored_write(void* ioapic, uint8_t entry, uint8_t vector, uint8_t delivery, uint8_t destmode, uint8_t polarity, uint8_t mode, uint8_t mask, uint8_t dest) {
    uint32_t value = vector
        | (delivery & 0b111) << 8
        | (destmode & 1) << 11
        | (polarity & 1) << 13
        | (mode & 1) << 15
        | (mask & 1) << 16;

    ioapic_write(ioapic, (0x10 + entry * 2) & 0xff, value);
    ioapic_write(ioapic, (0x11 + entry * 2) & 0xff, ((uint32_t) dest) << 24);
}

static void lapic_write(enum lapic_register reg, uint32_t value) {
    volatile uint32_t* ptr = (void*) ((uintptr_t) lapic_addr + reg);
    *ptr = value;
}

void apic_init(void) {
    madt = (struct MADT*) acpi_find_header("APIC");
    assert(madt);

    list_start = (void*) ((uintptr_t) madt + sizeof(struct MADT));
    list_end   = (void*) ((uintptr_t) madt + madt->header.length);

    override_count  = entry_count(MADT_TYPE_OVERRIDE);
    io_count        = entry_count(MADT_TYPE_IOAPIC);
    lapic_count     = entry_count(MADT_TYPE_LAPIC);
    lapic_nmi_count = entry_count(MADT_TYPE_LANMI);

    struct lapic_override* lapic64 = (struct lapic_override*) get_entry(MADT_TYPE_64BITADDR, 0);

    lapic_phys_addr = lapic64 ? (void*) lapic64->addr : (void*) (uintptr_t) madt->addr;

    lapic_addr = vmm_map(nullptr, PAGE_SIZE, VMM_FLAGS_PHYSICAL, MMU_FLAGS_READ | MMU_FLAGS_WRITE | MMU_FLAGS_NOEXEC, lapic_phys_addr);
    assert(lapic_addr);

    io_apics = kcalloc(io_count, sizeof(struct io_apic_desc));
    assert(io_apics);

    for(size_t i = 0; i < io_count; i++) {
        struct io_entry* entry = (struct io_entry*) get_entry(MADT_TYPE_IOAPIC, i);

        assert((entry->addr % PAGE_SIZE) == 0);
        io_apics[i].addr = vmm_map(nullptr, PAGE_SIZE, VMM_FLAGS_PHYSICAL, MMU_FLAGS_READ | MMU_FLAGS_WRITE | MMU_FLAGS_NOEXEC, (void*) (uintptr_t) entry->addr);
        assert(io_apics[i].addr);

        io_apics[i].base = entry->intbase;
        io_apics[i].top = io_apics[i].base + ((ioapic_read(io_apics[i].addr, IOAPIC_REG_ENTRYCOUNT) >> 16) & 0xff) + 1;
        klog(DEBUG, "ioapic (%zu): addr %p -- base %d -- top %d", i, (void*) (uintptr_t) entry->addr, entry->intbase, io_apics[i].top);
        for(int j = io_apics[i].base; j < io_apics[i].top; j++)
            iored_write(io_apics[i].addr, j - io_apics[i].base, 0xfe, 0, 0, 0, 0, 1, 0);
    }
}

void apic_send_eoi(__unused uint32_t irq) {
    lapic_write(APIC_REG_EOI, 0);
}

size_t apic_lapic_count(void) {
    return lapic_count;
}

size_t apic_get_lapic_entries(struct lapic_entry* entries, size_t n) {
    struct apic_list_header* entry = list_start;
    size_t i = 0;

    while(entry < list_end && i <= n) {
        if(entry->type == MADT_TYPE_LAPIC)
            entries[i++] = * (struct lapic_entry*) entry;
        entry = next_entry(entry);
    }

    return i;
}

