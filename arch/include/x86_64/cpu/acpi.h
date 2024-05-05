#ifndef _AMETHYST_X86_64_CPU_ACPI_H
#define _AMETHYST_X86_64_CPU_ACPI_H

#include <stdint.h>

enum ACPI_revision : uint8_t {
    ACPI_V1,
    ACPI_V2 = 2
};

struct RSDP {
    char sig[8];
    uint8_t checksum;
    char oem[6];
    enum ACPI_revision revision;
    uint32_t rsdt;
    uint32_t length;
    uint64_t xsdt;
    uint8_t xchecksum;
    uint8_t reserved[3];
} __attribute__((packed));

struct SDT_header {
    char sig[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

struct XSDT {
    struct SDT_header header;
    uint64_t tables[];
} __attribute__((packed));

struct RSDT {
    struct SDT_header header;
    uint32_t tables[];
} __attribute__((packed));

bool acpi_validate_sdt(struct SDT_header* header);
void acpi_init(void);

#endif /* _AMETHYST_X86_64_CPU_ACPI_H */

