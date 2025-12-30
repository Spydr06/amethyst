#include <sys/syscall.h>

#include <errno.h>
#include <string.h>
#include <version.h>
#include <mem/user.h>

static int getutsname(struct utsname* dest) {
    strncpy(dest->sysname, "amethyst", __UTS_LEN);
    strncpy(dest->nodename, "(none)", __UTS_LEN);
    strncpy(dest->version, AMETHYST_VERSION, __UTS_LEN);
    strncpy(dest->machine, "x86_64", __UTS_LEN);
    return 0;
}

__syscall syscallret_t _sys_uname(struct cpu_context*, struct utsname* u_name) {
    syscallret_t ret = {
        .ret = 0
    };

    if(!u_name) {
        ret._errno = EINVAL;
        return ret;
    }

    struct utsname utsname;
    if((ret._errno = getutsname(&utsname)))
        return ret;

    ret._errno = memcpy_to_user(u_name, &utsname, sizeof(struct utsname));
    return ret;
}

_SYSCALL_REGISTER(SYS_uname, _sys_uname, "uname", "%p");
