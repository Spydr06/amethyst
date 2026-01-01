#include <sys/mount.h>

#include "syscall.h"

int mount(const char *source, const char *target, const char *fstype, unsigned long flags, const void *data) {
    return __syscall(SYS_mount, source, target, fstype, flags, data);
}

int umount(const char *target) {
    return __syscall(SYS_umount, target);
}

