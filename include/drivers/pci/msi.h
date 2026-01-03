#ifndef _AMETHYST_DRIVERS_PCI_MSI_H
#define _AMETHYST_DRIVERS_PCI_MSI_H

#include <drivers/pci/pci.h>

#define MSIX_BIR_MASK ((uint32_t) 0x03)

struct msix_capability {
    uint32_t table_offset;
    uint32_t pending_bit_offset;
};

static_assert(sizeof(struct msix_capability) == sizeof(uint32_t) * 2);

int pci_enable_msi(struct pci_device *device);
int pci_enable_msix(struct pci_device *device);

#endif /* _AMETHYST_DRIVERS_PCI_MSI_H */

