#include <amethyst/module.h>
#include <amethyst/amethyst.h>

static int test_main(int argc, const char **argv) {
    printk("[Test Module] Hello, World!\n");
    return 0;
}

static void test_cleanup(void) {
    printk("[Test Module] Exit.\n");
}

_MODULE_REGISTER(
    _MODULE_INFO("test", "MIT", "0.0.1", "Basic Test Module"),
    .main_func = test_main,
    .cleanup_func = test_cleanup
);

