#include <cpu/cpu.h>
#include <cpu/syscalls.h>
#include <drivers/pci/pci.h>
#include <drivers/video/vga.h>
#include <filesystem/device.h>
#include <filesystem/initrd.h>
#include <filesystem/temporary.h>
#include <filesystem/virtual.h>
#include <init/cmdline.h>
#include <io/tty.h>
#include <mem/heap.h>
#include <mem/vmm.h>
#include <sys/scheduler.h>
#include <sys/subsystems/shard.h>
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

void thread1_func(void) {
    while(1) {
        klog(ERROR, "thread 1 on cpu #%d", _cpu()->id);
       sched_sleep(1'000'000);
    }
}

void thread2_func(void) {
    sched_sleep(500'000);

    while(1) {
        klog(ERROR, "thread 2 on cpu #%d", _cpu()->id);
       sched_sleep(1'000'000);
    }
}

static void sched_test(void) {
    klog(INFO, "testing scheduler...");
    struct thread* thread1 = sched_new_thread(thread1_func, PAGE_SIZE * 16, 0, nullptr, nullptr);
    assert(thread1);
    sched_queue(thread1);

    struct thread* thread2 = sched_new_thread(thread2_func, PAGE_SIZE * 16, 0, nullptr, nullptr);
    assert(thread2);
    sched_queue(thread2);
}

void kmain(size_t cmdline_size, const char* cmdline)
{
    syscalls_init(); 
     
    vmm_cache_init();

    vfs_init();
    tmpfs_init();
    devfs_init();

    tty_init();

    pci_init();
    
    cmdline_parse(cmdline_size, cmdline);
    
    create_ttys(); 
//    initrd_unpack();

    greet();
    color_test();

    klog(INFO, "\e[4mHello, World\e[24m %.02f", 3.14); 
    
    shard_subsystem_init();

    sched_test();

    // let the scheduler take over
    sched_stop_thread();
}

