#include <drivers/pci/pci.h>

#include <kernelio.h>
#include <assert.h>
#include <dynarray.h>

#ifdef __x86_64__
#include <x86-common/dev/io.h>
#define PCI_DATA_PORT    0x0cfc
#define PCI_COMMAND_PORT 0x0cf8
#endif

#define MAX_DEVICE 32
#define MAX_FUNCTION 8

struct dynarray pci_devices;

static void pci_check_bus(uint8_t bus, uint64_t parent);

static void get_address(struct pci_device* device, uint32_t offset) {
    uint32_t address = (device->bus << 16)
        | (device->device << 11)
        | (device->func << 8)
        | (offset & ~((uint32_t) 3))
        | 0x80000000;
    outl(PCI_COMMAND_PORT, address);
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
    uint32_t config_3c = pci_device_read_dword(&device, 0x3c);

    device.parent = parent;
    device.device_id = (uint16_t) (config_0 >> 16);
    device.vendor_id = (uint16_t) config_0;
    device.rev_id = (uint8_t) config_8;
    device.subclass = (uint8_t) (config_8 >> 16);
    device.device_class = (uint8_t) (config_8 >> 24);
    device.prog_if = (uint8_t) (config_8 >> 8);
    device.irq_pin = (uint8_t) (config_3c >> 8);

    if(config_c & 0x800000)
        device.multifunction = 1;
    else
        device.multifunction = 0;

    size_t id = dynarr_push(&pci_devices, &device);
    if(device.device_class == 0x06 && device.subclass == 0x04) {
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

    klog(INFO, "PCI Scan: found %zu devices:", pci_devices.size);

    for(size_t i = 0; i < pci_devices.size; i++) {
        struct pci_device* dev = dynarr_getelem(&pci_devices, i);

        const struct pci_vendor_id* vendor_id = pci_lookup_vendor_id(dev->vendor_id);
        const struct pci_device_id* dev_id = pci_lookup_device_id(vendor_id, dev->device_id);

        klog(INFO, 
            "  %02hhx:%02hhx.%01hhx - %s (%04hx) : %s (%04hx)",
            dev->bus,
            dev->device,
            dev->func,
            vendor_id ? vendor_id->vendor : "unknown",
            dev->vendor_id,
            dev_id ? dev_id->device : "unknown",
            dev->device_id
        );
    }
}

uint32_t pci_device_read_dword(struct pci_device* device, uint32_t offset) {
    assert(!(offset & 1));
    get_address(device, offset);
    return inl(PCI_DATA_PORT + (offset & 3));
}

void pci_device_write_dword(struct pci_device* device, uint32_t offset, uint32_t value) {
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


