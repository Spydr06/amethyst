#include <tty.h>
#include <multiboot.h>
#include <kernelio.h>

#include <stdint.h>

void kmain(void)
{
    console_init();
    
    if(multiboot_sig != MULTIBOOT_BOOTLOADER_MAGIC) {
        printk("Corrupted multiboot information.\nGot: %p\n", (void*) (uintptr_t) multiboot_sig);
        return;
    }

    multiboot_info_t* multiboot_info = (void*) (uintptr_t) multiboot_ptr;        

    printk("Booting from %s...\n", (const char*) (uintptr_t) multiboot_info->boot_loader_name);

    printk("Framebuffer of type %hhu at %p [%xx%x:%u]\n",
            multiboot_info->framebuffer_type, 
            (void*) (uintptr_t) multiboot_info->framebuffer_addr, 
            multiboot_info->framebuffer_width, multiboot_info->framebuffer_height,
            multiboot_info->framebuffer_bpp);

     

    printk("Hello, World!\n");
    while(1);
}
