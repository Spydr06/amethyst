#ifndef _AMETHYST_DRIVERS_ACPI_H
#define _AMETHYST_DRIVERS_ACPI_H

#include <stdint.h>

enum acpi_revision : uint8_t {
    ACPI_V1,
    ACPI_V2 = 2
};

struct acpi_generic_address {
    uint8_t address_space;
    uint8_t bit_width;
    uint8_t bit_offset;
    uint8_t access_size;
    uint64_t address;
} __attribute__((packed));

struct rsdp {
    char sig[8];
    uint8_t checksum;
    char oem[6];
    enum acpi_revision revision;
    uint32_t rsdt;
    uint32_t length;
    uint64_t xsdt;
    uint8_t xchecksum;
    uint8_t reserved[3];
} __attribute__((packed));

void acpi_init(void);

#endif /* _AMETHYST_DRIVERS_ACPI_H */

