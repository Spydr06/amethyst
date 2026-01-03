#ifndef _AMETHYST_DRIVERS_PCI_MANAGER_H
#define _AMETHYST_DRIVERS_PCI_MANAGER_H

#include <drivers/pci/pci.h>
#include <sys/spinlock.h>

struct pci_driver {
    struct pci_device *device;
    spinlock_t driver_lock;

    void *userp;
};

struct pci_driver_spec {
    enum pci_class class;
    enum pci_subclass subclass;

    void *userp;

    bool (*accept_device)(const struct pci_device *device);
    int (*driver_instantiate)(struct pci_driver *self);
};

int pci_manager_load_driver(const struct pci_driver_spec *driver);

void pci_manager_init(void);

#endif /* _AMETHYST_DRIVERS_PCI_MANAGER_H */

