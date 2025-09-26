#include <drivers/acpi/acpi.h>

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

static size_t header_count;
static struct sdt_header** headers;

static struct fadt* fadt;

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
        assert(acpi_validate_sdt(&rsdt->header));

        header_count = (rsdt->header.length - sizeof(struct sdt_header)) / sizeof(uint32_t);
        headers = kcalloc(header_count, sizeof(void*));
        assert(headers);
        
        for(size_t i = 0; i < header_count; i++) {
            void* addr = MAKE_HHDM(rsdt->tables[i]);
            if(acpi_validate_sdt(addr))
                headers[i] = addr;
        }
    }
    else if(rsdp->revision == ACPI_V2) {
        klog(INFO, "\e[95mACPI version 2.0\e[0m");
        
        struct xsdt* xsdt = MAKE_HHDM(rsdp->xsdt);
        assert(acpi_validate_sdt(&xsdt->header));

        header_count = (xsdt->header.length - sizeof(struct sdt_header)) / sizeof(uint64_t);
        headers = kcalloc(header_count, sizeof(void*));
        assert(headers);

        for(size_t i = 0; i < header_count; i++) {
            void* addr = MAKE_HHDM(xsdt->tables[i]);
            if(acpi_validate_sdt(addr))
                headers[i] = addr;
        }
    }
    else
        panic("Unknown ACPI version: %hhu.0", rsdp->revision);

#ifndef NDEBUG
    klog(DEBUG, "ACPI headers:");

    for(size_t i = 0; i < header_count; i++) {
        if(!headers[i])
            continue;

        klog(DEBUG, " %2zu) %.4s : %.6s : %.8s", i, headers[i]->sig, headers[i]->oem_id, headers[i]->oem_table_id);
    }
#endif

    struct sdt_header* fadt_header = acpi_find_header("FACP");
    if(fadt_header) {
        klog(DEBUG, "\"FACP\" table found.");
        fadt = (struct fadt*) fadt_header;
    }
    else {
        klog(WARN, "No \"FACP\" table found.");
    }
}

bool acpi_validate_sdt(struct sdt_header* header) {
    uint8_t* ptr = (uint8_t*) header;
    uint8_t sum = 0;
    for(unsigned i = 0; i < header->length; i++)
        sum += *ptr++;

    return sum == 0;
}

struct sdt_header* acpi_find_header(const char* sig) {
    for(size_t i = 0; i < header_count; i++) {
        if(!headers[i])
            continue;
        
        if(!strncmp(sig, headers[i]->sig, 4))
            return headers[i];
    }

    return nullptr;
}

struct fadt* acpi_get_fadt(void) {
    return fadt;
}

