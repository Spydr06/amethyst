#ifndef _AMETHYST_DRIVERS_ACPI_HPET_H
#define _AMETHYST_DRIVERS_ACPI_HPET_H

#define HPET_ACPI_HEADER_SIG "HPET"

#include <time.h>

#include "tables.h"

struct hpet_addressing {
    uint8_t address_space_id;
    uint8_t register_bit_width;
    uint8_t register_bit_offset;
    uint8_t __reserved;
    uint64_t address;
} __attribute__((packed));

int hpet_init(const struct sdt_header *header);

bool hpet_exists(void);
time_t hpet_ticks_per_us(void);

void hpet_wait_us(time_t us);
time_t hpet_ticks(void);

#endif /* _AMETHYST_DRIVERS_ACPI_HPET_H */

