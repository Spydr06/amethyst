#include <sys/syscall.h>
#include <unistd.h>

void __amethyst_yield(void) {
    syscall(SYS_yield);
}

