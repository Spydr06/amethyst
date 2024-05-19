#ifndef _AMETHYST_X86_64_DEV_HPET_H
#define _AMETHYST_X86_64_DEV_HPET_H

#define HPET_ACPI_HEADER_SIG "HPET"

#include <x86_64/cpu/acpi.h>

#include <time.h>

struct HPET_addressing {
    uint8_t address_space_id;
    uint8_t register_bit_width;
    uint8_t register_bit_offset;
    uint8_t __reserved;
    uint64_t address;
} __attribute__((packed));

struct HPET {
    struct SDT_header header;

    uint8_t hardware_rev_id;
    uint8_t comparator_count   : 5;
    uint8_t counter_size       : 1;
    uint8_t __reserved         : 1;
    uint8_t legacy_replacement : 1;

    uint16_t pci_vendor_id;
    uint8_t addr_id;
    uint8_t bit_width;
    uint8_t bit_offset;
    uint8_t __reserved2;
    uint64_t addr;
    uint8_t hpet_number;
    uint16_t minimum_tick;
    uint8_t page_protection;
} __attribute__((packed));

time_t hpet_init(void);

void hpet_wait_us(time_t us);
time_t hpet_ticks(void);

#endif /* _AMETHYST_X86_64_DEV_HPET_H */

