#ifndef _SYS_MOUNT_H
#define _SYS_MOUNT_H

int mount(const char *source, const char *target, const char *fstype, unsigned long mountflags, const void *data);
int umount(const char *target);

#endif /* _SYS_MOUNT_H */

