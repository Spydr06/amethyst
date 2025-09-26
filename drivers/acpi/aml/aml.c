#include <drivers/acpi/aml.h>

#include <mem/pmm.h>

#include <assert.h>
#include <errno.h>

static struct dsdt *load_dsdt(struct fadt *fadt) {
    struct dsdt* dsdt = MAKE_HHDM(fadt->dsdt);
    assert(acpi_validate_sdt(&dsdt->header));

    return dsdt;
}

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

int acpi_load_aml(void) {
    struct fadt *fadt = acpi_get_fadt();
    if(!fadt) {
        klog(ERROR, "no \"FADT\" table found, cannot load AML.");
        return ENOENT;
    }

    struct dsdt *dsdt = load_dsdt(fadt);
    assert(dsdt != nullptr);

    int err = aml_init(dsdt);
    if(err)
        return err;

    if((err = aml_parse(dsdt->definition_block, dsdt->header.length))) {
        klog(ERROR, "failed to parse AML from DSDT: %s", aml_strerror(err));
        return EINVAL;
    }

    return 0;
}

