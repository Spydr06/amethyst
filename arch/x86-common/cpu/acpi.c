#include "acpi.h"
#include "rsdt.h"

#include <kernelio.h>
#include <mem/bitmap.h>
#include <mem/vmm.h>

#include <string.h>

static struct RSDT* rsdt_root = nullptr;
static struct XSDT* xsdt_root __attribute__((unused)) = nullptr;
static enum RSDT_version version = RSDT_V2;
static uint32_t rsdt_tables_total = 0;

static bool parse_rsdt(const struct RSDP_descriptor* desc) {
    klog(DEBUG, "- RSDT descriptor address: %p", (void*) (uintptr_t) desc->rsdt_address);
    map_phys_to_virt_addr(
        (void*) ALIGN_PHYSADDRESS((uintptr_t) desc->rsdt_address),
        (void*) ENSURE_HIGHER_HALF((uintptr_t) desc->rsdt_address),
        VMM_FLAGS_PRESENT | VMM_FLAGS_WRITE_ENABLE
    );

    bitmap_set_bit_from_address(ALIGN_PHYSADDRESS((uintptr_t) desc->rsdt_address));
    rsdt_root = (struct RSDT*) ENSURE_HIGHER_HALF((uintptr_t) desc->rsdt_address);
    struct ACPI_SDT_header header = rsdt_root->header;

#ifndef NDEBUG
    char sig[5] = {0};
    memcpy(sig, header.signature, sizeof(header.signature));
    klog(DEBUG, "- RSDT Signature: \"%s\"; Header size: %u", sig, header.length);
#endif

    version = RSDT_V1;

    size_t extra_pages = (header.length / PAGE_SIZE) + 1;
    if(extra_pages > 1) {
        for(size_t i = 0; i < extra_pages; i++) {
            uintptr_t new_phys_addr = desc->rsdt_address + (i * PAGE_SIZE);
            map_phys_to_virt_addr(
                (void*) ALIGN_PHYSADDRESS(new_phys_addr),
                (void*) ENSURE_HIGHER_HALF(new_phys_addr),
                VMM_FLAGS_PRESENT | VMM_FLAGS_WRITE_ENABLE
            );
            bitmap_set_bit_from_address(ALIGN_PHYSADDRESS(new_phys_addr));
        }
    }

    rsdt_tables_total = (header.length - sizeof(struct ACPI_SDT_header)) / sizeof(uint32_t);
    klog(DEBUG, "- Total RSDT tables: %u", rsdt_tables_total);

    for(uint32_t i = 0; i < rsdt_tables_total; i++) {
        map_phys_to_virt_addr(
            (void*) ALIGN_PHYSADDRESS((uintptr_t) rsdt_root->tables[i]),
            (void*) ENSURE_HIGHER_HALF((uintptr_t) rsdt_root->tables[i]),
            VMM_FLAGS_PRESENT | VMM_FLAGS_WRITE_ENABLE
        );
#ifndef NDEBUG
        struct ACPI_SDT_header* table_header = (struct ACPI_SDT_header*) ENSURE_HIGHER_HALF((uintptr_t) rsdt_root->tables[i]);
        memcpy(sig, table_header->signature, sizeof(table_header->signature));
        klog(DEBUG, "  (%2u): Signature: %s", i, sig);
#endif
    }

    return acpi_validate_sdt((uint8_t*) desc, sizeof(struct RSDP_descriptor));
}

static bool parse_rsdt_v2(const struct RSDP_descriptor_20* desc __attribute__((unused))) {
    panic("unimplemented - parsing RSDT descriptors version 2 is not supported yet.");
}

bool acpi_parse_sdt(uintptr_t address, enum RSDT_version type) {
    switch(type) {
        case RSDT_V1:
            return parse_rsdt((struct RSDP_descriptor*) address);
        case RSDT_V2:
            return parse_rsdt_v2((struct RSDP_descriptor_20*) address);
        default:
            panic("unimplemented - No such RSDT version %hhu", type);
    }
}

bool acpi_validate_sdt(uint8_t* descriptor, size_t size) {
    uint32_t sum = 0;
    for(size_t i = 0; i < size; i++) {
        sum += ((uint8_t*) descriptor)[i]; 
    }
    
    klog(DEBUG, "Checksum of RSDP is 0x%02x", sum & 0xff);

    return (sum & 0xff) == 0;
}

