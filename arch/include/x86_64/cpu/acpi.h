#ifndef _AMETHYST_X86_64_CPU_ACPI_H
#define _AMETHYST_X86_64_CPU_ACPI_H

#include <stdint.h>
#include <stddef.h>

enum RSDT_version : uint8_t {
    RSDT_V1 = 14,
    RSDT_V2
};

struct ACPI_SDT_header {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oemid[6];
    char oemtableid[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

struct RSDP_descriptor {
    char signature[8];
    uint8_t checksum;
    char oemid[6];
    uint8_t revision;
    uint32_t rsdt_address;
} __attribute__((packed));

struct RSDP_descriptor_20 {
    struct RSDP_descriptor first_part;

    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t __reserved[3];
} __attribute__((packed));

bool acpi_parse_sdt(uintptr_t address, enum RSDT_version type);
bool acpi_validate_sdt(uint8_t* descriptor, size_t size);

#endif /* _AMETHYST_X86_64_CPU_ACPI_H */

