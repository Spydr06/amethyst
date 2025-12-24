#ifndef _AMETHYST_DRIVERS_PCI_H
#define _AMETHYST_DRIVERS_PCI_H

#include <stdint.h>
#include <stddef.h>

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

#endif /* _AMETHYST_DRIVERS_PCI_H */

