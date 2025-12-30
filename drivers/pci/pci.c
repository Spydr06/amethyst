#include "drivers/pci/pci_manager.h"
#include <drivers/pci/pci.h>

#include <mem/heap.h>

#include <errno.h>
#include <kernelio.h>
#include <assert.h>
#include <dynarray.h>

#ifdef __x86_64__
    #include <x86_64/dev/io.h>
    #define PCI_DATA_PORT    0x0cfc
    #define PCI_COMMAND_PORT 0x0cf8
#endif

#define MAX_DEVICE 32
#define MAX_FUNCTION 8

struct dynarray pci_devices;

static void pci_check_bus(uint8_t bus, uint64_t parent);

static void get_address(const struct pci_device* device, uint32_t offset) {
    uint32_t address = (device->bus << 16)
        | (device->device << 11)
        | (device->func << 8)
        | (offset & 0xfc)
        | 0x80000000;
    outl(PCI_COMMAND_PORT, address);
}

static void pci_device_load_default_header(struct pci_device* device) {
    struct pci_default_header* header = &device->header.ext_default;
    uint32_t* header_words = (uint32_t*) header;

    for(size_t i = 0; i < sizeof(struct pci_default_header) / sizeof(uint32_t); i++) {
        header_words[i] = pci_device_read_dword(device, PCI_EXTRA_HEADER_OFFSET + i*  sizeof(uint32_t));
    }
}

static void pci_device_load_pci_bridge_header(struct pci_device* device) {
    struct pci_bridge_header* header = &device->header.ext_bridge;
    uint32_t* header_words = (uint32_t*) header;

    for(size_t i = 0; i < sizeof(struct pci_bridge_header) / sizeof(uint32_t); i++) {
        header_words[i] = pci_device_read_dword(device, PCI_EXTRA_HEADER_OFFSET + i*  sizeof(uint32_t));
    }
}

static void pci_device_load_cardbus_bridge_header(struct pci_device* device) {
    struct pci_cardbus_header* header = &device->header.ext_cardbus;
    uint32_t* header_words = (uint32_t*) header;

    for(size_t i = 0; i < sizeof(struct pci_cardbus_header) / sizeof(uint32_t); i++) {
        header_words[i] = pci_device_read_dword(device, PCI_EXTRA_HEADER_OFFSET + i*  sizeof(uint32_t));
    }
}

static void pci_check_function(uint8_t bus, uint8_t slot, uint8_t func, int64_t parent) {
    struct pci_device device = {0};
    device.bus = bus;
    device.func = func;
    device.device = slot;

    uint32_t config_0 = pci_device_read_dword(&device, 0x00);

    if(config_0 == 0xffffffff)
        return;

    uint32_t config_8 = pci_device_read_dword(&device, 0x08);
    uint32_t config_c = pci_device_read_dword(&device, 0x0c);

    device.parent = parent;
    device.header.device_id = (uint16_t) (config_0 >> 16);
    device.header.vendor_id = (uint16_t) config_0;
    device.header.class = (uint8_t) (config_8 >> 24);
    device.header.subclass = (uint8_t) (config_8 >> 16);
    device.header.prog_if = (uint8_t) (config_8 >> 8);
    device.header.rev_id = (uint8_t) config_8;
    device.header.type = (uint8_t) (config_c >> 16);

    switch(device.header.type) {
        case PCI_HEADER_GENERAL:
            pci_device_load_default_header(&device);
            break;
        case PCI_HEADER_PCI_TO_PCI_BRIDGE:
            pci_device_load_pci_bridge_header(&device);
            break;
        case PCI_HEADER_PCI_TO_CARDBUS_BRIDGE:
            pci_device_load_cardbus_bridge_header(&device);
            break;
        default:
            klog(ERROR, "Unsupported header type %02hhx", device.header.type);
    }

    size_t id = dynarr_push(&pci_devices, &device);
    if(device.header.class == PCI_CLASS_BRIDGE && device.header.subclass == PCI_SUB_PCI_TO_PCI) {
        // PCI to PCI bridge
        uint32_t config_18 = pci_device_read_dword(&device, 0x18);
        pci_check_bus((config_18 >> 8) & 0xff, id);
    }
}

static void pci_check_bus(uint8_t bus, uint64_t parent) {
    for(size_t dev = 0; dev < MAX_DEVICE; dev++) {
        for(size_t func = 0; func < MAX_FUNCTION; func++) {
            pci_check_function(bus, dev, func, parent);
        }
    }
}

static void pci_init_root_bus(void) {
    struct pci_device device = {0};
    uint32_t config_c = pci_device_read_dword(&device, 0x0c);
    uint32_t config_0;

    if(!(config_c & 0x800000)) {
        pci_check_bus(0, -1);
        return;
    }
    
    for(size_t func = 0; func < MAX_FUNCTION; func++) {
        device.func = func;
        config_0 = pci_device_read_dword(&device, 0);
        if(config_0 & 0xffffffff)
            continue;

        pci_check_bus(func, -1);
    }
}

void pci_init(void) {
    dynarr_init(&pci_devices, sizeof(struct pci_device), 0);

    pci_init_root_bus();

    klog(INFO, "\e[95mPCI Scan:\e[0m found %zu devices:", pci_devices.size);

    for(size_t i = 0; i < pci_devices.size; i++) {
        struct pci_device* dev = dynarr_getelem(&pci_devices, i);

        const struct pci_vendor_id* vendor_id = pci_lookup_vendor_id(dev->header.vendor_id);
        const struct pci_device_id* dev_id = pci_lookup_device_id(vendor_id, dev->header.device_id);

        klog(INFO, 
            "  %02hhx:%02hhx.%01hhx - %s (%04hx) : %s (%04hx) [%02hhx:%02hhx:%02hhx:%02hhx]",
            dev->bus,
            dev->device,
            dev->func,
            vendor_id ? vendor_id->vendor : "unknown",
            dev->header.vendor_id,
            dev_id ? dev_id->device : "unknown",
            dev->header.device_id,
            dev->header.class,
            dev->header.subclass,
            dev->header.prog_if,
            dev->header.rev_id
        );
    }

    pci_manager_init();
}

uint16_t pci_device_get_status(const struct pci_device* device) {
    uint32_t config_4 = pci_device_read_dword(device, 0x04);
    return (uint16_t) ((config_4 >> 16) & 0xffff);
}

static int read_capabilities(struct pci_device* device, uint8_t offset) {
    struct pci_capability* capability = kmalloc(sizeof(struct pci_capability));
    if(!capability)
        return ENOMEM;

    uint32_t* cap_dword = (uint32_t*) capability;
    *cap_dword = pci_device_read_dword(device, offset);

    capability->pci_offset = offset;
    capability->next = device->capabilities;
    device->capabilities = capability;

    klog(DEBUG, "Capability: %hhx, next: %hhx", capability->cap_id, capability->next_ptr);

    if(capability->next_ptr)
        read_capabilities(device, capability->next_ptr);

    return 0;
}

int pci_device_load_capabilities(struct pci_device* device) {
    spinlock_acquire(&device->lock);

    int err = 0;

    if(device->capabilities)
        goto cleanup;

    if(!pci_device_has_capabilities(device)) {
        err = ENOENT;
        goto cleanup;
    }

    uint8_t cap_ptr ;
    switch(device->header.type) {
    case PCI_HEADER_GENERAL:
        cap_ptr = device->header.ext_default.capabilities_ptr;
        break;
    case PCI_HEADER_PCI_TO_PCI_BRIDGE:
        cap_ptr = device->header.ext_bridge.capabilities_ptr;
        break;
    case PCI_HEADER_PCI_TO_CARDBUS_BRIDGE:
        cap_ptr = device->header.ext_cardbus.capabilities_offset;
        break;
    default:
        err = EINVAL;
        goto cleanup;
    }

    cap_ptr &= 0xfc;
    if((err = read_capabilities(device, cap_ptr)))
        goto cleanup;

cleanup:
    spinlock_release(&device->lock);
    return err;
}

struct pci_capability* pci_device_get_capability(struct pci_device* device, enum pci_capability_id cap_id) {
    if(pci_device_load_capabilities(device))
        return nullptr; // error loading capabilities

    struct pci_capability *cap = device->capabilities;
    
    while(cap) {
        if(cap->cap_id == cap_id)
            return cap;

        cap = cap->next;
    }

    return nullptr;
}

uint32_t pci_device_read_dword(const struct pci_device* device, uint32_t offset) {
    assert(!(offset & 1));
    get_address(device, offset);
    return inl(PCI_DATA_PORT + (offset & 3));
}

void pci_device_write_dword(const struct pci_device* device, uint32_t offset, uint32_t value) {
    assert(!(offset & 3));
    get_address(device, offset);
    outl(PCI_DATA_PORT + (offset & 3), value);
}

const struct pci_vendor_id* pci_lookup_vendor_id(uint16_t vendor_id) {
    for(size_t i = 0; i < pci_id_lookup_table_size; i++) {
        if(pci_id_lookup_table[i].vendor_id == vendor_id)
            return &pci_id_lookup_table[i];
    } 
    return nullptr;
}

const struct pci_device_id* pci_lookup_device_id(const struct pci_vendor_id* vendor, uint16_t device_id) {
    if(!vendor)
        return nullptr;

    for(size_t i = 0; i < vendor->num_device_ids; i++) {
        if(vendor->device_ids[i].device_id == device_id)
            return &vendor->device_ids[i];
    }

    return nullptr;
}


