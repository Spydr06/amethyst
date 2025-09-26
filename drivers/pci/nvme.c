#include "mem/slab.h"
#include "mem/vmm.h"
#include "sys/spinlock.h"
#include "x86_64/mem/mmu.h"
#include <drivers/pci/msi.h>
#include <drivers/pci/pci.h>
#include <drivers/pci/nvme.h>

#include <drivers/pci/pci_manager.h>

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <kernelio.h>

static struct scache* driver_cache;

static int nvme_instantiate(struct pci_driver* driver);
static bool nvme_accept(const struct pci_device* device);

static const struct pci_driver_spec nvme_driver_spec = {
    .class = PCI_CLASS_MASS_STORAGE,
    .subclass = PCI_SUB_NONVOLATILE_MEMORY_CONTROLLER,

    .accept_device = nvme_accept,
    .driver_instantiate = nvme_instantiate,
};

static uint64_t nvme_base_addr(uint32_t bar1, uint32_t bar0) {
    return (uint64_t)(((uint64_t) (bar1 & 0xffffffff) << 32) | (bar0 & 0xfffffff0));
}

/*static inline uint32_t nvme_read_register(uint32_t offset) {
    volatile uint32_t* nvme_reg = (volatile uint32_t* )(NVME_BASE_ADDR + offset);
}*/

static bool nvme_accept(const struct pci_device* device) {
    return device->header.class == PCI_CLASS_MASS_STORAGE &&
        device->header.subclass == PCI_SUB_NONVOLATILE_MEMORY_CONTROLLER &&
        device->header.prog_if == PCI_PROG_IF_NVME;
}

static void nvme_reset(struct nvme_device* device) {
    klog(DEBUG, "controller reset");
    device->base_registers->cc &= ~1;

    while(nvme_ready(device->base_registers->csts));
}

static int nvme_init_device(struct nvme_device* device, uint64_t base_addr, struct pci_device* pci_device) {
    spinlock_acquire(&device->device_lock);

    int err = 0;

    void* mapped_addr = vmm_map(nullptr, PAGE_SIZE, VMM_FLAGS_PHYSICAL, MMU_FLAGS_WRITE | MMU_FLAGS_READ | MMU_FLAGS_NOEXEC, (void*) base_addr);
    if(!mapped_addr) {
        err = ENOMEM;
        goto cleanup;
    }

    klog(INFO, "NVME base address: %p, mmio: %p", (void*) base_addr, mapped_addr);

    device->base_addr = base_addr;
    device->mmio_addr = mapped_addr;
    device->capability_stride = (uint8_t) ((base_addr >> 12) & 0x0f);

    struct nvme_version ver = nvme_get_version(device->base_registers->vs);
    klog(INFO, "NVME version: %d.%d.%d", ver.major, ver.minor, ver.tertiary);

    // check capabilities
    
    size_t min_pagesize = nvme_get_min_pagesize(device->base_registers->cap);
    size_t max_pagesize = nvme_get_max_pagesize(device->base_registers->cap);
    klog(INFO, "NVME page size: %zu to %zu", min_pagesize, max_pagesize);

    if(min_pagesize > PAGE_SIZE || max_pagesize < PAGE_SIZE) {
        klog(ERROR, "controller does not the required page size of %u bytes", PAGE_SIZE);
        err = ENODEV;
        goto cleanup;
    }

    enum nvme_commandset commandset = nvme_get_commandset(device->base_registers->cap);

    if(!(commandset & NVME_COMMANDSET_NVM)) {
        klog(ERROR, "controller does not support the NVM command set");
        err = ENODEV;
        goto cleanup;
    }

    // enable interrupts
    
    if((err = pci_enable_msi(pci_device)))
        goto cleanup;

    // reset controller

    nvme_reset(device);

cleanup:
    spinlock_release(&device->device_lock);
    return err;
}

static int nvme_instantiate(struct pci_driver* driver) {
    klog(INFO, "Loading driver for device %d", driver->device->header.device_id);

    if(driver->device->header.type != PCI_HEADER_GENERAL)
        return ENODEV;

    const struct pci_default_header* header = &driver->device->header.ext_default;
    uint64_t bar = nvme_base_addr(header->bar[1], header->bar[0]);
    assert(bar % PAGE_SIZE == 0);

    struct nvme_device* device = slab_alloc(driver_cache);
    spinlock_init(device->device_lock);

    int err;
    if((err = nvme_init_device(device, bar, driver->device))) {
        slab_free(driver_cache, device);
        return err;
    }

    driver->userp = device;

    return 0;
}

void nvme_init(void) {
    driver_cache = slab_newcache(sizeof(struct nvme_device), alignof(struct nvme_device), nullptr, nullptr);
    assert(driver_cache != nullptr);

    int err;
    if((err = pci_manager_load_driver(&nvme_driver_spec))) {
        klog(ERROR, "Failed registering NVME driver: %s", strerror(err));
        return;
    }
}

