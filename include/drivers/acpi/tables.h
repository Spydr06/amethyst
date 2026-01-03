#ifndef _AMETHYST_DRIVERS_ACPI_TABLES_H
#define _AMETHYST_DRIVERS_ACPI_TABLES_H

#include <stdint.h>

#include "acpi.h"

#define ACPI_MAX_TABLES 64

typedef char sdt_signature_t[4];

struct sdt_header {
    sdt_signature_t sig;
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

struct xsdt {
    struct sdt_header header;
    uint64_t tables[];
} __attribute__((packed));

struct rsdt {
    struct sdt_header header;
    uint32_t tables[];
} __attribute__((packed));

struct fadt {
    struct sdt_header header;

    uint32_t firmware_control;
    uint32_t dsdt;

    uint8_t __reserved;

    uint8_t preferred_power_management_profile;
    uint16_t sci_interrupt;
    uint32_t smi_commandport;
    uint8_t acpi_enable;
    uint8_t acpi_disable;
    uint8_t s4bios_req;
    uint8_t pstate_control;
    uint32_t pm1a_event_block;
    uint32_t pm1b_event_block;
    uint32_t pm1a_control_block;
    uint32_t pm1b_control_block;
    uint32_t pm2_controlblock;
    uint32_t pm_timer_block;
    uint32_t gpe0_block;
    uint32_t gpe1_block;
    uint8_t  pm1_event_length;
    uint8_t  pm1_control_length;
    uint8_t  pm2_control_length;
    uint8_t  pm_timer_length;
    uint8_t  gpe0_length;
    uint8_t  gpe1_ength;
    uint8_t  gpe1_base;
    uint8_t  c_state_control;
    uint16_t worst_c2_latency;
    uint16_t worst_c3_latency;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t  duty_offset;
    uint8_t  duty_width;
    uint8_t  day_alarm;
    uint8_t  month_alarm;
    uint8_t  century;

    // reserved in ACPI 1.0; used since ACPI 2.0+
    uint16_t boot_architecture_flags;

    uint8_t  __reserved2;
    uint32_t flags;

    // 12 byte structure; see below for details
    struct acpi_generic_address retset_reg;

    uint8_t  reset_value;
    uint8_t  __reserved3[3];

    // 64bit pointers - Available on ACPI 2.0+
    uint64_t                x_firmware_control;
    uint64_t                x_dsdt;

    struct acpi_generic_address x_pm1a_event_block;
    struct acpi_generic_address x_pm1b_event_block;
    struct acpi_generic_address x_pm1a_control_block;
    struct acpi_generic_address x_pm1b_control_block;
    struct acpi_generic_address x_pm2_control_block;
    struct acpi_generic_address x_pm_timer_block;
    struct acpi_generic_address x_gpe0_block;
    struct acpi_generic_address x_gpe1_block;
} __attribute__((packed));

struct madt {
    struct sdt_header header;
    uint32_t addr;
    uint32_t flags;
    uint8_t entries[];
} __attribute__((packed));

enum madt_entry_type : uint8_t {
    MADT_TYPE_LAPIC,
    MADT_TYPE_IOAPIC,
    MADT_TYPE_OVERRIDE,
    MADT_TYPE_IONMI,
    MADT_TYPE_LANMI,
    MADT_TYPE_64BITADDR,
    MADT_TYPE_X2APIC = 9
};

struct dsdt {
    struct sdt_header header;
    uint8_t definition_block[];
} __attribute__((packed));

struct ssdt {
    struct sdt_header header;
    uint8_t definition_block[];
} __attribute__((packed));

struct hpet {
    struct sdt_header header;

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

bool acpi_validate_sdt(const struct sdt_header* header);

int acpi_register_table(struct sdt_header* header);
struct sdt_header* acpi_find_table(sdt_signature_t sig);

void acpi_print_tables(void);

#endif /* _AMETHYST_DRIVERS_ACPI_TABLES_H */
