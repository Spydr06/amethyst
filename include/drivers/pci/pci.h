#ifndef _AMETHYST_DRIVERS_PCI_H
#define _AMETHYST_DRIVERS_PCI_H

#include <stdint.h>

struct pci_driver_identifier {
    uint16_t vendor;
    uint16_t device;
    uint16_t class;
    uint16_t subclass;
    uint16_t interface;
} __attribute__((packed));

struct pci_device_descriptor {
    uint32_t bar0;
    uint32_t bar1;
    uint32_t bar2;
    uint32_t bar3;
    uint32_t interrupt;

    uint16_t bus;
    uint16_t device;
    uint16_t function;

    uint16_t vendor_id;
    uint16_t device_id;

    uint8_t class_id;
    uint8_t subclass_id;
    uint8_t interface_id;

    uint8_t revision;
};

struct pci_base_address_register {
    bool prefetchable;
    uint8_t type;
    uint32_t size;
    uint8_t* address;
};

struct pci {
    uint16_t data_port;
    uint16_t command_port;
    struct pci_device_descriptor dev;
    struct pci_base_address_register bar;
};

extern struct pci pci;

void pci_init(uint16_t data_port, uint16_t command_port);

bool pci_find_driver(struct pci_driver_identifier identifier);
bool pci_find_driver_on_bus(struct pci_driver_identifier identifier, uint16_t bus, uint16_t device);

void pci_get_device_descriptor(uint16_t bus, uint16_t device, uint16_t function);
void pci_get_base_address_register(uint16_t bus, uint16_t device, uint16_t function, uint16_t bar_num);

uint32_t pci_read(uint16_t bus, uint16_t device, uint16_t function, uint32_t register_offset);
void pci_write(uint16_t bus, uint16_t device, uint16_t function, uint32_t register_offset, uint32_t value);

inline uint32_t pci_read_device(struct pci_device_descriptor device, uint32_t register_offset) {
    return pci_read(device.bus, device.device, device.function, register_offset);
}

inline void pci_write_device(struct pci_device_descriptor device, uint32_t register_offset, uint32_t value) {
    return pci_write(device.bus, device.device, device.function, register_offset, value);
}

inline struct pci_device_descriptor pci_get_descriptor(void) {
    return pci.dev;
}

#endif /* _AMETHYST_DRIVERS_PCI_H */

