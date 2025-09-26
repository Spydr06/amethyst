#ifndef _DRIVERS_ACPI_AML_H
#define _DRIVERS_ACPI_AML_H

#include <drivers/acpi/acpi.h>

struct dsdt {
    struct sdt_header header;
    uint8_t definition_block[];
} __attribute__((packed));

enum aml_error {
    AML_OK = 0
};

int acpi_load_aml(void);

#endif /* _DRIVERS_ACPI_AML_H */

