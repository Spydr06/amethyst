#include <unistd.h>
#include <stdarg.h>
#include <errno.h>

#include <sys/syscall.h>
#include <internal/syscall.h>

#undef syscall

long syscall(long number, ...) {
    va_list ap;
    syscall_arg_t args[__SYSCALL_MAX_ARGS];

    va_start(ap, number);
    for(int i = 0; i < __SYSCALL_MAX_ARGS; i++)
        args[i] = va_arg(ap, syscall_arg_t);
    va_end(ap);

    return __syscall_ret(__syscall(number, args[0], args[1], args[2], args[3], args[4], args[5]));
}

long __syscall_ret(unsigned long r)
{
    if(r > -4096UL) {
        errno = -r;
        return -1;
    }

    return r;
}

