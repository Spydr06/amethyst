#include <cpu/cpu.h>
#include <drivers/pci/pci.h>
#include <drivers/video/vga.h>
#include <filesystem/device.h>
#include <filesystem/initrd.h>
#include <filesystem/temporary.h>
#include <filesystem/virtual.h>
#include <init/cmdline.h>
#include <io/pseudo_devices.h>
#include <io/tty.h>
#include <mem/heap.h>
#include <mem/vmm.h>
#include <sys/scheduler.h>
#include <sys/subsystems/shard.h>
#include <sys/syscall.h>
#include <sys/tty.h>

#include <kernelio.h>
#include <assert.h>
#include <version.h>

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
    printk("\n");

    /*for(int i = 0; i < 256; i++) {
        printk("\e[48;5;%hhum  \e[0m", i);
        
        if(i == 15 || (i > 15 && i < 232 && (i - 16) % 36 == 35) || i == 231)
            printk("\n");
    }*/

    printk("\n\n");
}

void kmain(size_t cmdline_size, const char* cmdline)
{
    cmdline_parse(cmdline_size, cmdline);
    
    vmm_cache_init();

    vfs_init();
    tmpfs_init();
    devfs_init();

    pseudodevices_init();

    tty_init();
    create_ttys(); 

    pci_init(); 

    // const char* root = cmdline_get("root"); // root device
    const char* rootfs = cmdline_get("rootfs"); // root filesystem
    if(!rootfs)
        panic("no `rootfs=` flag in the kernel command line found, please specify a root filesystem");

    klog(INFO, "mounting root (%s) on `/`", rootfs);

    assert(vfs_mount(nullptr, vfs_root, "/", rootfs, nullptr) == 0);

    initrd_unpack();

    greet();
    color_test();

    klog(INFO, "\e[4mHello, World\e[24m %.02f", 3.14); 

    shard_subsystem_init();

    int err = scheduler_exec(
        "/bin/init", 1,
        (char*[]){"/bin/init", nullptr}, // argc
        (char*[]){nullptr}               // envp
    );

    if(err)
        panic("Error executing `/bin/init`: %s", strerror(err));

    // let the scheduler take over
    sched_stop_thread();
}

