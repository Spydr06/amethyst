#include <drivers/acpi/acpi.h>
#include <drivers/acpi/tables.h>

#include <limine.h>

#include <kernelio.h>
#include <assert.h>
#include <math.h>

#include <mem/pmm.h>
#include <mem/vmm.h>
#include <mem/heap.h>

static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST,
    .revision = 0
};

void acpi_init(void) {
    if(!rsdp_request.response)
        panic("No ACPI information received from bootloader");

    struct rsdp* rsdp = rsdp_request.response->address;

    uintptr_t page_offset = (uintptr_t) rsdp % PAGE_SIZE;

    // ensure the rsdp doesn't cross a page boundary
    assert(ROUND_DOWN((uintptr_t) rsdp, PAGE_SIZE) == ROUND_DOWN((uintptr_t) rsdp - 1 + sizeof(struct rsdp), PAGE_SIZE));
    
    void* virtual = vmm_map(nullptr, sizeof(struct rsdp), VMM_FLAGS_PHYSICAL, MMU_FLAGS_READ | MMU_FLAGS_WRITE, FROM_HHDM(rsdp));
    assert(virtual);

    rsdp = (struct rsdp*)((uintptr_t) virtual + page_offset);

    if(rsdp->revision == ACPI_V1) {
        klog(INFO, "\e[95mACPI version 1.0\e[0m");

        struct rsdt* rsdt = MAKE_HHDM(rsdp->rsdt);
        int err = acpi_register_table(&rsdt->header);
        if(err)
            panic("Could not register ACPI RSDT: %s", strerror(err));
    }
    else if(rsdp->revision == ACPI_V2) {
        klog(INFO, "\e[95mACPI version 2.0\e[0m");
        
        struct xsdt* xsdt = MAKE_HHDM(rsdp->xsdt);
        int err = acpi_register_table(&xsdt->header);
        if(err)
            panic("Could not register ACPI XSDT: %s", strerror(err));
    }
    else
        panic("Unknown ACPI version: %hhu.0", rsdp->revision);

#ifndef NDEBUG
    klog(DEBUG, "ACPI headers:");

    acpi_print_tables();
#endif
}

