#include <drivers/pci/pci.h>
#include <kernelio.h>
#include <drivers/pci/msi.h>

#include <errno.h>

int pci_enable_msix(struct pci_device *device) {
    struct pci_capability *msix_cap = pci_device_get_capability(device, PCI_CAP_MSI_X);
    if(!msix_cap) {
        klog(WARN, "PCI device %d: No support for MSI or MSI-X", device->header.device_id);
        return ENODEV;
    }

    klog(DEBUG, "PCI device %d: Enabling MSI-X", device->header.device_id);

    struct msix_capability extra_cap = {
        .table_offset = pci_device_read_dword(device, msix_cap->pci_offset + sizeof(uint32_t)),
        .pending_bit_offset = pci_device_read_dword(device, msix_cap->pci_offset + 2 * sizeof(uint32_t))
    };

    klog(DEBUG, "MSI-X: table offset: %x, pending bit offset: %x", extra_cap.table_offset, extra_cap.pending_bit_offset);

    return 0;
}

int pci_enable_msi(struct pci_device *device) {
    struct pci_capability *msi_cap = pci_device_get_capability(device, PCI_CAP_MSI);
    if(!msi_cap)
        return pci_enable_msix(device);

    unimplemented();

    return 0;
}

