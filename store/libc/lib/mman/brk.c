#include <unistd.h>
#include <sys/syscall.h>

int brk(void* addr) {
    return syscall(SYS_brk, addr) < 0 ? -1 : 0;
}

void* sbrk(intptr_t increment) {
    intptr_t current_brk = syscall(SYS_brk, 0);
    if(current_brk == -1) 
        return (void*) -1;

    return (void*) syscall(SYS_brk, current_brk + increment);
}

