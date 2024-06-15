#ifndef _AMETHYST_X86_64_APIC_H
#define _AMETHYST_X86_64_APIC_H

#include <stddef.h>
#include <stdint.h>

#include <x86_64/cpu/acpi.h>

struct apic_list_header {
    enum MADT_entry_type type;
    uint8_t length;
} __attribute__((packed));

struct lapic_override {
	struct apic_list_header header;
	uint16_t reserved;
	uint64_t addr;
} __attribute__((packed)); 

struct io_entry {
	struct apic_list_header header;
	uint8_t id;
	uint8_t reserved;
	uint32_t addr;
	uint32_t intbase;
} __attribute__((packed));

struct lapic_entry {
	struct apic_list_header header;
	uint8_t acpiid;
	uint8_t lapicid;
	uint32_t flags;
} __attribute__((packed));

struct lapic_nmi_entry {
	struct apic_list_header header;
	uint8_t acpiid;
	uint16_t flags;
	uint8_t lint;
} __attribute__((packed));

struct io_apic_override {
	struct apic_list_header header;
	uint8_t bus;
	uint8_t irq;
	uint32_t gsi;
	uint16_t flags;
} __attribute__((packed));

struct io_apic_desc {
    void* addr;
    int base;
    int top;
};

enum io_apic_register : uint8_t {
    IOAPIC_REG_ID,
    IOAPIC_REG_ENTRYCOUNT,
    IOAPIC_REG_PRIORITY,
    IOAPIC_REG_ENTRY = 0x10
};

enum lapic_register : uint32_t {
	APIC_REG_ID = 0x20,
	APIC_REG_EOI = 0xB0,
	APIC_REG_SPURIOUS = 0xF0,
	APIC_REG_ICR_LO = 0x300,
	APIC_REG_ICR_LO_STATUS = 0x1000,
	APIC_REG_ICR_HI = 0x310,

    APIC_LVT_TIMER = 0x320,
    APIC_LVT_THERMAL = 0x330,
    APIC_LVT_PERFORMANCE = 0x340,
    APIC_LVT_LINT0 = 0x350,
    APIC_LVT_LINT1 = 0x360,
    APIC_LVT_ERROR = 0x370,

    APIC_TIMER_INITIAL_COUNT = 0x380,
    APIC_TIMER_COUNT = 0x390,
    APIC_TIMER_DIVIDE = 0x3e0
};

extern void *lapic_addr, *lapic_phys_addr;

void apic_init(void);
void apic_initap(void);
void apic_timer_init(void);

void apic_send_eoi(uint32_t irq);

size_t apic_lapic_count(void);
size_t apic_get_lapic_entries(struct lapic_entry* entries, size_t n);

void ioapic_register_interrupt(uint8_t irq, uint8_t vector, uint8_t proc, bool masked);

#endif /* _AMETHYST_X86_64_APIC_H */
