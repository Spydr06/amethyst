#include <errno.h>
#include <threads.h>

int* __errno_location(void) {
    static thread_local int errno_val = 0;
    return &errno_val;
}

