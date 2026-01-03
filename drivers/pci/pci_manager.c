#include "mem/slab.h"
#include <drivers/pci/pci_manager.h>

#include <memory.h>
#include <assert.h>
#include <dynarray.h>

extern struct dynarray pci_devices;

static struct scache *driver_cache;

static spinlock_t drivers_lock;
static struct dynarray drivers;

static int instantiate(struct pci_driver *driver, const struct pci_driver_spec *spec, const struct pci_device *device) {
    memset(driver, 0, sizeof(struct pci_driver));
    spinlock_init(driver->driver_lock);

    driver->device = device;
    driver->userp = spec->userp;

    return spec->driver_instantiate(driver);
}

int pci_manager_load_driver(const struct pci_driver_spec *spec) {
    spinlock_acquire(&drivers_lock);

    assert(spec->accept_device != nullptr);
    int err = 0;

    for(size_t i = 0; i < pci_devices.size; i++) {
        struct pci_device *device = dynarr_getelem(&pci_devices, i);

        if(!spec->accept_device(device))
            continue;
        
        struct pci_driver *driver = slab_alloc(driver_cache);
        if((err = instantiate(driver, spec, device))) {
            slab_free(driver_cache, driver);
            goto cleanup;
        }

        dynarr_push(&drivers, driver);
    }

cleanup:
    spinlock_release(&drivers_lock);
    return err;
}

void pci_manager_init(void) {
    spinlock_init(drivers_lock);

    dynarr_init(&drivers, sizeof(struct pci_driver), 128);
    driver_cache = slab_newcache(sizeof(struct pci_driver), alignof(struct pci_driver), nullptr, nullptr);
    assert(driver_cache != nullptr);
}
