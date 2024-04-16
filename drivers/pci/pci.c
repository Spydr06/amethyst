#include <drivers/pci/pci.h>
#include "arch/x86-common/io.h"
#include "kernelio.h"

#include <stdint.h>
#include <string.h>

struct pci pci;

void pci_write_device(struct pci_device_descriptor device, uint32_t register_offset, uint32_t value);
uint32_t pci_read_device(struct pci_device_descriptor device, uint32_t register_offset); 
struct pci_device_descriptor pci_get_descriptor(void);

void pci_init(uint16_t data_port, uint16_t command_port) {
    memset(&pci, 0, sizeof(struct pci));
    pci.data_port = data_port;
    pci.command_port = command_port;
}

uint32_t pci_read(uint16_t bus, uint16_t device, uint16_t function, uint32_t register_offset) {
    uint32_t id = (0x1 << 31)
        | ((bus & 0xff) << 16)
        | ((device & 0x1f) << 11)
        | ((function & 0x07) << 8)
        | (register_offset & 0xfc);

    outl(pci.command_port, id);
    uint32_t dev = inl(pci.data_port);
    return dev >> (8 * (register_offset % 4));
}

void pci_write(uint16_t bus, uint16_t device, uint16_t function, uint32_t register_offset, uint32_t value) {
    uint32_t id = (0x1 << 31)
        | ((bus & 0xff) << 16)
        | ((device & 0x1f) << 11)
        | ((function & 0x07) << 8)
        | (register_offset & 0xfc);

    outl(pci.command_port, id);
    outl(pci.data_port, value);
}

void pci_get_base_address_register(uint16_t bus, uint16_t device, uint16_t function, uint16_t bar_num) {
    uint32_t headertype = pci_read(bus, device, function, 0x0e) & 0x7f;
    int32_t max_bars = 6 - 4 * headertype;
    if(bar_num >= max_bars)
        return;

    uint32_t bar_value = pci_read(bus, device, function, 0x10 + 4 * bar_num);
    pci.bar.type = bar_value & 0x1 ? 1 : 0;

    if(!pci.bar.type) {
        // TODO
        pci.bar.address = (uint8_t*)(uintptr_t)(bar_value & ~0x7);
    }
    else if(bar_value & 0x1) {
        pci.bar.address = (uint8_t*)(uintptr_t)(bar_value & ~0x3);
        pci.bar.prefetchable = false;
    }

    return;
}

bool pci_find_driver(struct pci_driver_identifier identifier) {
    for(uint16_t bus = 0; bus < 8; bus++)
        for(uint16_t device = 0; device < 32; device++)
            if(pci_find_driver_on_bus(identifier, bus, device))
                return true;
    return false;
}

bool pci_device_has_function(uint16_t bus, uint16_t device) {
    return pci_read(bus, device, 0, 0x0e) & (1 << 7); 
}

bool pci_find_driver_on_bus(struct pci_driver_identifier identifier, uint16_t bus, uint16_t device) {
    uint8_t functions = pci_device_has_function(bus, device) ? 8 : 1;
    for(uint8_t function = 0; function < functions; function++) {
        pci_get_device_descriptor(bus, device, function);
        if(pci.dev.vendor_id == 0x0000 || pci.dev.vendor_id == 0xffff)
            break;

        klog(DEBUG, "pci dev %04hx", pci.dev.device_id);

        for(uint16_t bar_num = 0; bar_num < 6; bar_num++) {
            pci_get_base_address_register(bus, device, function, bar_num);
            if(!pci.bar.address)
                continue;

            switch(bar_num) {
                case 0:
                    pci.dev.bar0 = (uintptr_t) pci.bar.address;
                    break;
                case 1:
                    pci.dev.bar1 = (uintptr_t) pci.bar.address;
                    break;
                case 2:
                    pci.dev.bar2 = (uintptr_t) pci.bar.address;
                    break;
                case 3:
                    pci.dev.bar3 = (uintptr_t) pci.bar.address;
                    break;
            }
        }

        if(pci.dev.vendor_id == identifier.vendor && pci.dev.device_id == identifier.device)
            return true;

        if((pci.dev.class_id == identifier.class && pci.dev.subclass_id == identifier.subclass)
            && (pci.dev.interface_id == identifier.interface || !identifier.interface))
                return true;
    }

    return false; 
}

void pci_get_device_descriptor(uint16_t bus, uint16_t device, uint16_t function) {
    pci.dev.bus = bus;
    pci.dev.device = device;
    pci.dev.function = function;

    pci.dev.vendor_id = pci_read(bus, device, function, 0x00);
    pci.dev.device_id = pci_read(bus, device, function, 0x02);

    pci.dev.class_id = pci_read(bus, device, function, 0x0b);
    pci.dev.subclass_id = pci_read(bus, device, function, 0x0a);
    pci.dev.interface_id = pci_read(bus, device, function, 0x09);

    pci.dev.revision = pci_read(bus, device, function, 0x08);
    pci.dev.interrupt = pci_read(bus, device, function, 0x3c);
}

