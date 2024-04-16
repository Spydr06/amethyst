#include "drivers/pci/pci.h"
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
}

void kmain(void)
{
    syscalls_init();
    
    struct pci_driver_identifier ata_ident = { 0, 0, 1, 1, 0 };
    if(!pci_find_driver(ata_ident))
        panic("Could not find ATA device on the PCI bus.");


    klog(INFO, "found ATA device %04hx", pci.dev.device_id);

    greet();
    color_test();
//    syscalls_init();
}

