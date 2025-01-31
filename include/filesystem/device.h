#ifndef _AMETHYST_FILESYSTEM_DEVICE_H
#define _AMETHYST_FILESYSTEM_DEVICE_H

#include <abi.h>
#include <filesystem/virtual.h>
#include <hashtable.h>

enum {
    DEV_MAJOR_NULL = 1,
	DEV_MAJOR_FULL,
	DEV_MAJOR_ZERO,
	DEV_MAJOR_TTY,
	DEV_MAJOR_FB,
	DEV_MAJOR_KEYBOARD,
	DEV_MAJOR_BLOCK,
	DEV_MAJOR_E9,
	DEV_MAJOR_URANDOM,
	DEV_MAJOR_NET,
	DEV_MAJOR_MOUSE,
	DEV_MAJOR_PTY
};

struct devops {
    int (*open)(int minor, struct vnode** vnode, int flags);
    int (*close)(int minor, int flags);
    int (*read)(int minor, void* buffer, size_t size, uintmax_t offset, int flags, size_t* readc);
    int (*write)(int minor, void* buffer, size_t size, uintmax_t offset, int flags, size_t* writec);
    // TODO: poll()?
    int (*mmap)(int minor, void* addr, uintmax_t offset, int flags);
    int (*munmap)(int minor, void* addr, uintmax_t offset, int flags);
    int (*isatty)(int minor);
    int (*ioctl)(int minor, uint64_t request, void* arg, int* result);
    int (*maxseek)(int minor, size_t* max);
    void (*inactive)(int minor);
};

struct dev_node {
    struct vnode vnode;
    struct vattr vattr;
    struct devops* devops;
    struct vnode* physical;
    struct dev_node* master;
    hashtable_t children;
};

void devfs_init(void);

int devfs_register(struct devops* decops, const char* name, int type, int major, int minor, mode_t mode);
void devfs_remove(const char* name, int major, int minor);

int devfs_get_root(struct vfs* vfs, struct vnode** node);
int devfs_getnode(struct vnode* physical, int major, int minor, struct vnode** node);
int devfs_lookup(struct vnode* node, const char* name, struct vnode** result, struct cred* cred);
int devfs_find(const char* name, struct vnode** dest);

int devfs_mount(struct vfs** vfs, struct vnode* mount_point, struct vnode* backing, void* data);
int devfs_unmount(struct vfs* vfs);
int devfs_create(struct vnode* parent, const char* name, struct vattr* attr, int type, struct vnode** result, struct cred* cred);

int devfs_open(struct vnode** nodep, int flags, struct cred* cred);
int devfs_close(struct vnode* node, int flags, struct cred* cred);

int devfs_read(struct vnode* node, void* buffer, size_t size, uintmax_t offset, int flags, size_t* bytes_read, struct cred* cred);
int devfs_write(struct vnode* node, void* buffer, size_t size, uintmax_t offset, int flags, size_t* bytes_written, struct cred* cred);

int devfs_setattr(struct vnode* node, struct vattr* attr, int which, struct cred* cred);
int devfs_getattr(struct vnode* node, struct vattr* attr, struct cred* cred);

int devfs_inactive(struct vnode* node);

#endif /* _AMETHYST_FILESYSTEM_DEVICE_H */

