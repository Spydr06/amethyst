#include <stdlib.h>

#include <sys/syscall.h>
#include <internal/syscall.h>

#define COUNT 32

struct fl {
    struct fl *next;
    void (*f[COUNT])(void);
};

// TODO: remove attribute when bss is working correctly
static struct fl __attribute__((section("data"))) builtin, *head;
static int __attribute__((section("data"))) slot;

static void funcs_on_exit(void) {
    for(; head; head = head->next, slot = COUNT)
        while(slot--)
            head->f[slot]();
}

_Noreturn void _Exit(int status) {
    while(1)
        __syscall(SYS_exit, status);
}

_Noreturn void exit(int status) {
    funcs_on_exit();
    _Exit(status);
}

int atexit(void (*func)(void)) {
    if(!head)
        head = &builtin;

    if(slot == COUNT) {
        // TODO: heap-allocate new fl nodes
        return -1;
    }
    
    head->f[slot++] = func;
    
    return 0;
}
