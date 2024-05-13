#ifndef _AMETHYST_X86_64_CPU_ACPI_H
#define _AMETHYST_X86_64_CPU_ACPI_H

#include <stdint.h>

enum ACPI_revision : uint8_t {
    ACPI_V1,
    ACPI_V2 = 2
};

struct ACPI_generic_address {
    uint8_t address_space;
    uint8_t bit_width;
    uint8_t bit_offset;
    uint8_t access_size;
    uint64_t address;
} __attribute__((packed));

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

struct FADT {
    struct SDT_header header;

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
    struct ACPI_generic_address retset_reg;

    uint8_t  reset_value;
    uint8_t  __reserved3[3];

    // 64bit pointers - Available on ACPI 2.0+
    uint64_t                x_firmware_control;
    uint64_t                x_dsdt;

    struct ACPI_generic_address x_pm1a_event_block;
    struct ACPI_generic_address x_pm1b_event_block;
    struct ACPI_generic_address x_pm1a_control_block;
    struct ACPI_generic_address x_pm1b_control_block;
    struct ACPI_generic_address x_pm2_control_block;
    struct ACPI_generic_address x_pm_timer_block;
    struct ACPI_generic_address x_gpe0_block;
    struct ACPI_generic_address x_gpe1_block;
} __attribute__((packed));

struct MADT {
    struct SDT_header header;
    uint32_t addr;
    uint32_t flags;
    uint8_t entries[];
} __attribute__((packed));

enum MADT_entry_type : uint8_t {
    MADT_TYPE_LAPIC,
    MADT_TYPE_IOAPIC,
    MADT_TYPE_OVERRIDE,
    MADT_TYPE_IONMI,
    MADT_TYPE_LANMI,
    MADT_TYPE_64BITADDR,
    MADT_TYPE_X2APIC = 9
};

void acpi_init(void);

struct SDT_header* acpi_find_header(const char* sig);
bool acpi_validate_sdt(struct SDT_header* header);

struct FADT* acpi_get_fadt(void);

#endif /* _AMETHYST_X86_64_CPU_ACPI_H */

