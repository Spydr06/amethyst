#include <sys/stat.h>

#include <sys/syscall.h>
#include <unistd.h>

int mkdir(const char *pathname, mode_t mode) {
    return syscall(SYS_mkdir, pathname, mode);
}

