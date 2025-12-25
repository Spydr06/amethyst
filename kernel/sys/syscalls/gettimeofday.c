#include <sys/syscall.h>
#include <sys/timekeeper.h>

#include <mem/user.h>

#include <errno.h>

__syscall syscallret_t _sys_gettimeofday(struct cpu_context* __unused, struct timeval *tv_ptr) {
    syscallret_t ret = {
        .ret = 0,
        ._errno = -1
    };

    if(!tv_ptr) {
        ret._errno = EINVAL;
        return ret;
    }

    struct timespec ts = timekeeper_time();

    struct timeval tv = {
        .tv_sec = ts.s,
        .tv_usec = ts.ns / 1000ul,
    };

    if(memcpy_to_user(tv_ptr, &tv, sizeof(struct timeval))) {
        ret._errno = EINVAL;
        return ret;
    }

    return ret;
}

_SYSCALL_REGISTER(SYS_gettimeofday, _sys_gettimeofday, "gettimeofday", "%p");
