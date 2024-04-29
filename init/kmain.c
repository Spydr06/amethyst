#include "filesystem/virtual.h"
#include <mem/heap.h>
#include <drivers/pci/pci.h>
#include <kernelio.h>
#include <version.h>
#include <cpu/syscalls.h>

static void greet(void) {
    printk("\n \e[1;32m>>\e[0m Booting \e[95mAmethyst\e[0m version \e[97m" AMETHYST_VERSION "\e[90m (built " AMETHYST_COMPILATION_DATE " " AMETHYST_COMPILATION_TIME ")\e[1;32m <<\e[0m\n");
}

static void color_test(void) {
    for(int i = 40; i <= 47; i++) {
        printk("\e[%im  \e[0m ", i);
    }
    for(int i = 100; i <= 107; i++) {
        printk("\e[%im  \e[0m ", i);
    }
    printk("\n\n");

    for(int i = 0; i < 256; i++) {
        printk("\e[48;5;%hhum  \e[0m", i);
        
        if(i == 15 || (i > 15 && i < 232 && (i - 16) % 36 == 35) || i == 231)
            printk("\n");
    }

    printk("\n\n");
}

void kmain(void)
{
    syscalls_init();
    
    kernel_heap_init();   
    pci_init();

    greet();
    color_test();

    vfs_init();
    while(1);
}

