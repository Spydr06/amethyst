#include <tty.h>
#include <multiboot2.h>
#include <kernelio.h>

#include "cdefs.h"
#include "video/vga.h"
#include <stdint.h>

void greet(void) {
    printk("Hello, World!\n");
}

uint32_t vga_buffer[INIT_VGA_WIDTH * INIT_VGA_HEIGHT];

void kmain(void)
{
    console_init();
    
    if(multiboot_sig != MULTIBOOT2_BOOTLOADER_MAGIC) {
        printk("Corrupted multiboot information.\nGot: %p\n", (void*) (uintptr_t) multiboot_sig);
        return;
    }

/*    multiboot_info_t* multiboot_info = __low_ptr(multiboot_ptr);
    printk("Multiboot info at %p\n", multiboot_info); 
    printk("Booting from %s...\n", (char*)__low_ptr(multiboot_info->boot_loader_name));

    printk("Framebuffer of type %hhu at %p [%xx%x:%u]\n",
            multiboot_info->framebuffer_type, 
            __low_ptr(multiboot_info->framebuffer_addr), 
            multiboot_info->framebuffer_width, multiboot_info->framebuffer_height,
            multiboot_info->framebuffer_bpp);
    struct vga vga;
    //vga_init(&vga, multiboot_info, vga_buffer);

    vga_put_pixel(&vga, 10, 10, 0xff0000ff);
//    vga_buffer_to_screen(&vga);*/

    greet(); 
    while(1);
}
