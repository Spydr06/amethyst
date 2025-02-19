#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <amethyst/ioctl.h>
#include <amethyst/fb.h>

int ioctl(int fd, unsigned long request, void *arg);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_IOCTL_H */

