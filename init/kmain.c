#include "filesystem/virtual.h"
#include <mem/heap.h>
#include <drivers/pci/pci.h>
#include <drivers/storage/ata.h>
#include <kernelio.h>
#include <version.h>
#include <cpu/syscalls.h>

static void greet(void) {
    printk("\n \e[1;32m>>\e[0m Booting \e[95mAmethyst\e[0m version \e[97m" AMETHYST_VERSION "\e[1;32m <<\e[0m\n");
}

static void color_test(void) {
    for(int i = 40; i <= 47; i++) {
        printk("\e[%im  \e[0m ", i);
    }
    for(int i = 100; i <= 107; i++) {
        printk("\e[%im  \e[0m ", i);
    }
    printk("\n\n");

    for(int i = 0; i < 255; i += 10) {
        printk("\e[48;2;%i;0;0m  \e[0m", i);
    }
    printk("\n\n");

    for(int i = 0; i < 255; i += 10) {
        printk("\e[48;2;0;%i;0m  \e[0m", i);
    }
    printk("\n\n");

    for(int i = 0; i < 255; i += 10) {
        printk("\e[48;2;0;0;%im  \e[0m", i);
    }
    printk("\n\n");



    printk("\n\n");
}

static void ata_init(struct pci_device_descriptor device) {
    klog(INFO, "Found ATA device %04hx", device.device_id);
}

static void ata_fail(void) {
    klog(WARN, "No ATA device found.");
}

static void pci_devices_init(void) {
    struct {
        struct pci_driver_identifier identifier;
        void (*callback)(struct pci_device_descriptor device);
        void (*on_error)(void);
    } pci_drivers[] = {
        {ata_identifier(), ata_init, ata_fail}
    };
    
    for(size_t i = 0; i < __len(pci_drivers); i++) {
        if(!pci_find_driver(pci_drivers[i].identifier)) {
            if(pci_drivers[i].on_error)
                pci_drivers[i].on_error();
            continue;
        }
            
        pci_drivers[i].callback(pci_get_descriptor());
    }
} 

void kmain(void)
{
    kernel_heap_init();
    pci_devices_init();
    syscalls_init();
    
    greet();
    color_test();

    vfs_init();

    while(1);
}

