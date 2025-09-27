#include <drivers/acpi/aml.h>
#include <drivers/acpi/tables.h>

#include <mem/pmm.h>

#include <kernelio.h>
#include <errno.h>
#include <memory.h>

static int aml_init(struct dsdt *dsdt) {
    klog(DEBUG, "\"%.4s\" : %.6s : %.8s", dsdt->header.sig, dsdt->header.oem_id, dsdt->header.oem_table_id);

    size_t aml_size = dsdt->header.length - sizeof(struct dsdt);
    klog(INFO, "DSDT table contains %zu bytes AML code", aml_size);

    return 0;
}

static enum aml_error aml_parse(const uint8_t *definition_block, size_t block_size) {
    return AML_OK;
}

static const char *aml_strerror(enum aml_error err) {
    switch(err) {
    default:
        return "<unknown error>";
    }
}

static int load_from_dsdt(struct dsdt* dsdt) {
    int err = aml_init(dsdt);
    if(err)
        return err;

    if((err = aml_parse(dsdt->definition_block, dsdt->header.length))) {
        klog(ERROR, "failed to parse AML from DSDT: %s", aml_strerror(err));
        return EINVAL;
    }

    return 0; 
}

static int load_from_ssdt(struct ssdt* ssdt) {
    return 0;
}

int acpi_load_aml(const struct sdt_header *header) {
    if(memcmp(header->sig, "DSDT", sizeof(sdt_signature_t)) == 0)
        return load_from_dsdt((struct dsdt*) header);
    if(memcmp(header->sig, "SSDT", sizeof(sdt_signature_t)) == 0)
        return load_from_ssdt((struct ssdt*) header);

    klog(WARN, "acpi_load_aml() called with unexpected ACPI header \"%.4s\"", header->sig);
    return EINVAL;
}

