#include <unistd.h>
#include <errno.h>
#include <sys/syscall.h>

char *getcwd(char *buf, size_t size) {
    errno = 0;
    syscall(SYS_getcwd, buf, size);
    return errno ? NULL : buf;
}
