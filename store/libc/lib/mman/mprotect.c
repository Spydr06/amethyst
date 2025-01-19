#include <sys/mman.h>

int mprotect(void *addr, size_t len, int prot) {
    (void) addr;
    (void) len;
    (void) prot;
    // TODO: implement with syscall
    return 0;
}
