#ifndef _AMETHYST_DRIVERS_STORAGE_ATA_H
#define _AMETHYST_DRIVERS_STORAGE_ATA_H

#include <drivers/pci/pci.h>

inline struct pci_driver_identifier ata_identifier(void) {
    return (struct pci_driver_identifier){ 0, 0, 1, 1, 0 };
}

#endif /* _AMETHYST_DRIVERS_STORAGE_ATA_H */

