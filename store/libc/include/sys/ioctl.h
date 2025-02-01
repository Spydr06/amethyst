#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H

#ifdef __cplusplus
extern "C" {
#endif

#define FBIOGET_VSCREENINFO 0x4600
#define FBIOGET_FSCREENINFO 0x4602

int ioctl(int fd, unsigned long request, void *arg);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_IOCTL_H */

