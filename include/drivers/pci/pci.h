#ifndef _AMETHYST_DRIVERS_PCI_H
#define _AMETHYST_DRIVERS_PCI_H

#include "dynarray.h"

#define PCI_DATA_PORT    0x0cfc
#define PCI_COMMAND_PORT 0x0cf8

struct pci_device {
    int64_t parent;
    uint8_t bus;
    uint8_t func;
    uint8_t device;
    uint16_t device_id;
    uint16_t vendor_id;
    uint8_t rev_id;
    uint8_t subclass;
    uint8_t device_class;
    uint8_t prog_if;
    int multifunction;
    uint8_t irq_pin;
    int has_prt;
    uint32_t gsi;
    uint16_t gsi_flags;
};

struct pic_bar {
    uintptr_t base;
    size_t size;

    bool is_mmio;
    bool is_prefetchable;
};

struct pci_device_id {
    uint16_t device_id;
    const char* device;
} __attribute__((packed));

struct pci_vendor_id {
    uint16_t vendor_id;
    const char* vendor;

    size_t num_device_ids;
    struct pci_device_id* device_ids;
} __attribute__((packed));

extern const size_t pci_id_lookup_table_size;
extern const struct pci_vendor_id pci_id_lookup_table[];

extern struct dynarray devices;

void pci_init(void);

uint32_t pci_device_read_dword(struct pci_device* device, uint32_t offset);
void pci_device_write_dword(struct pci_device* device, uint32_t offset, uint32_t value);

const struct pci_vendor_id* pci_lookup_vendor_id(uint16_t vendor_id);
const struct pci_device_id* pci_lookup_device_id(const struct pci_vendor_id*, uint16_t device_id);

#endif /* _AMETHYST_DRIVERS_PCI_H */

