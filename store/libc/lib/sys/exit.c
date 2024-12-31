#include <stdlib.h>

#include <sys/syscall.h>
#include <internal/syscall.h>

_Noreturn void _Exit(int status) {
    while(1)
        __syscall(SYS_exit, status);
}
