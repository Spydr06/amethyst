#include <x86_64/cpu/acpi.h>
#include <x86_64/cpu/rsdt.h>

#include <limine/limine.h>

#include <kernelio.h>
#include <assert.h>
#include <math.h>

#include <mem/pmm.h>
#include <mem/vmm.h>

static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST,
    .revision = 0
};

static size_t header_count = 0;
static enum RSDT_version rsdt_version; 
static union {
    struct XSDT** xsdt;
    struct RSDT** rsdt;
} headers;

void acpi_init(void) {
    if(!rsdp_request.response)
        panic("No ACPI information received from bootloader");

    struct RSDP_descriptor_20* rsdp = rsdp_request.response->address;

    uintptr_t page_offset = (uintptr_t) rsdp % PAGE_SIZE;

    // ensure the rsdp doesn't cross a page boundary
    assert(ROUND_DOWN((uintptr_t) rsdp, PAGE_SIZE) == ROUND_DOWN((uintptr_t) rsdp - 1 + sizeof(struct RSDP_descriptor_20), PAGE_SIZE));
    
    void* virtual = vmm_map(nullptr, sizeof(struct RSDP_descriptor_20), VMM_FLAGS_PHYSICAL, MMU_FLAGS_READ | MMU_FLAGS_WRITE, FROM_HHDM(rsdp));
    assert(virtual);

    rsdp = (struct RSDP_descriptor_20*)((uintptr_t) virtual + page_offset);

    rsdt_version = rsdp->header.revision;
    if(rsdp->header.revision == RSDT_V1) {
        klog(INFO, "\e[95mACPI version 1.0\e[0m");

        struct RSDT* rsdt = MAKE_HHDM(rsdp->xsdt_address);
        //assert(acpi_validate_sdt((uint8_t*) rsdt, sizeof(struct RSDP_descriptor)));
        
    }
    else if(rsdp->header.revision == RSDT_V2) {
        klog(INFO, "\e[95mACPI version 2.0\e[0m");
        
        struct XSDT* xsdt = MAKE_HHDM(rsdp->xsdt_address);
        //assert(acpi_validate_sdt((uint8_t*) xsdt, sizeof(struct RSDP_descriptor_20)));
    }
    else
        panic("Unknown ACPI version: %hhu.0",rsdp->header.revision);
}

bool acpi_validate_sdt(uint8_t* descriptor, size_t size) {
    uint32_t sum = 0;
    for(size_t i = 0; i < size; i++) {
        sum += ((uint8_t*) descriptor)[i]; 
    }
    
    klog(DEBUG, "checksum of RSDP is 0x%02x", sum & 0xff);

    return (sum & 0xff) == 0;
}

