#ifndef _DRIVERS_ACPI_AML_H
#define _DRIVERS_ACPI_AML_H

#include "tables.h"

enum aml_error {
    AML_OK = 0
};

int acpi_load_aml(const struct sdt_header *header);

#endif /* _DRIVERS_ACPI_AML_H */

