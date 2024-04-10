#include <tty.h>
#include <multiboot.h>
#include <kernelio.h>

#include <stdint.h>

void greet(void) {
    printk("Hello, World!\n");
}

void kmain(void)
{
    console_init();
    printk("sizeof multiboot_info_t: %zu\n", sizeof(multiboot_info_t));

    if(multiboot_sig != MULTIBOOT_BOOTLOADER_MAGIC) {
        printk("Corrupted multiboot information.\nGot: %p\n", (void*) (uintptr_t) multiboot_sig);
        return;
    }

    multiboot_info_t* multiboot_info = (multiboot_info_t*) (((uintptr_t) multiboot_ptr) - 0xFFFFFFFF80000000ull);
    printk("Multiboot info at %p\n", multiboot_info);

    printk("Booting from %s...\n", (const char*) (uintptr_t) multiboot_info->boot_loader_name);

    printk("Framebuffer of type %hhu at %p [%xx%x:%u]\n",
            multiboot_info->framebuffer_type, 
            (void*) (uintptr_t) multiboot_info->framebuffer_addr, 
            multiboot_info->framebuffer_width, multiboot_info->framebuffer_height,
            multiboot_info->framebuffer_bpp);
    greet(); 

    while(1);
}
