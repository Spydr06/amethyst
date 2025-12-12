#include <amethyst/module.h>
#include <amethyst/amethyst.h>

static int test_init(int argc, const char **argv) {
    printk("[Test Module] Hello, World!\n");
    return 0;
}

static void test_exit(void) {
    printk("[Test Module] Exit.\n");
}

