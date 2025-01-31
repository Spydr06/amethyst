#include <sys/mount.h>
#include <unistd.h>
#include <sys/syscall.h>

int mount(const char *source, const char *target, const char *fstype, unsigned long flags, const void *data) {
    return syscall(SYS_mount, source, target, fstype, flags, data);
}

int umount(const char *target) {
    return syscall(SYS_umount, target);
}

