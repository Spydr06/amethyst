#ifndef _AMETHYST_FILESYSTEM_VIRTUAL_H
#define _AMETHYST_FILESYSTEM_VIRTUAL_H

#include <abi.h>
#include <sys/mutex.h>

#include <stdint.h>
#include <stddef.h>

#define MAX_FILENAME_LENGTH 256

enum vflags {
    V_FLAGS_ROOT = 1
};

enum vfflags {
    V_FFLAGS_READ           = 1,
    V_FFLAGS_WRITE          = 2,
    V_FFLAGS_NONBLOCKING    = 4,
    V_FFLAGS_SHARED         = 8,
    V_FFLAGS_EXEC           = 16
};

enum vtype {
    V_TYPE_REGULAR,
    V_TYPE_DIR,
    V_TYPE_CHDEV,
    V_TYPE_BLKDEV,
    V_TYPE_FIFO,
    V_TYPE_LINK,
    V_TYPE_SOCKET
};

struct cred {
    uid_t uid;
    gid_t gid;
};

struct vattr {
    enum vtype type;
    mode_t mode;
    uid_t uid;
    gid_t gid;
    int fsid;
    ino_t inode;
    int nlinks;
    size_t size;
    size_t fsblock_size;
    struct timespec atime;
    struct timespec mtime;
    struct timespec ctime;
    int rdev_major;
    int rdev_minor;
    size_t blocks_used;
};

struct vfs {
    struct vfs* next;
    struct vfsops* ops;
    struct vnode* node_covered;
    struct vnode* root;
    enum vflags flags;
};

struct vfsops {
    int (*mount)(struct vfs **vfs, struct vnode *mp, struct vnode *backing, void *data);
	int (*unmount)(struct vfs *vfs);
	int (*sync)(struct vfs *vfs);
	int (*root)(struct vfs *vfs, struct vnode **root);
};

struct vnode {
    struct vops* ops;
    mutex_t lock;
    int refcount;
    enum vfflags flags;
    enum vtype type;
    struct vfs* vfs;
    struct vfs* vfsmounted;
    void* socketbinding;
};

struct polldata;

typedef struct vops {
	int (*open)(struct vnode **node, int flags, struct cred *cred);
	int (*close)(struct vnode *node, int flags, struct cred *cred);
	int (*read)(struct vnode *node, void *buffer, size_t size, uintmax_t offset, int flags, size_t *readc, struct cred *cred);
	int (*write)(struct vnode *node, void *buffer, size_t size, uintmax_t offset, int flags, size_t *writec, struct cred *cred);
	int (*lookup)(struct vnode *node, const char *name, struct vnode **result, struct cred *cred);
	int (*create)(struct vnode *parent, const char *name, struct vattr *attr, int type, struct vnode **result, struct cred *cred);
	int (*getattr)(struct vnode *node, struct vattr *attr, struct cred *cred);
	int (*setattr)(struct vnode *node, struct vattr *attr, struct cred *cred);
	int (*poll)(struct vnode *node, struct polldata *, int events);
	int (*access)(struct vnode *node, mode_t mode, struct cred *cred);
	int (*unlink)(struct vnode *node, const char *name, struct cred *cred);
	int (*link)(struct vnode *node, struct vnode *dir, const char *name, struct cred *cred);
	int (*symlink)(struct vnode *parent, const char *name, struct vattr *attr, char *path, struct cred *cred);
	int (*readlink)(struct vnode *parent, char **link, struct cred *cred);
	int (*inactive)(struct vnode *node);
	int (*mmap)(struct vnode *node, void *addr, uintmax_t offset, int flags, struct cred *cred);
	int (*munmap)(struct vnode *node, void *addr, uintmax_t offset, int flags, struct cred *cred);
	int (*getdents)(struct vnode *node, struct dent *buffer, size_t count, uintmax_t offset, size_t *readcount);
	int (*isatty)(struct vnode *node);
	int (*ioctl)(struct vnode *node, unsigned long request, void *arg, int *result);
	int (*maxseek)(struct vnode *node, size_t *max);
	int (*resize)(struct vnode *node, size_t newsize, struct cred *cred);
	int (*rename)(struct vnode *source, char *oldname, struct vnode *target, char *newname, int flags);
} vops_t;

enum vfs_lookup_flags {
    VFS_LOOKUP_PARENT = 0x20000000,
    VFS_LOOKUP_NOLINK = 0x40000000,
    VFS_LOOKUP_INTERNAL = 0x80000000
};

int vfs_lookup(struct vnode** dest, struct vnode* src, const char* path, char* last_comp, enum vfs_lookup_flags flags);

void vfs_init(void);

int vfs_register(struct vfsops* vfsops, const char* name);
int vfs_create(struct vnode* ref, const char* path, struct vattr* attr, enum vtype type, struct vnode** node);

void vfs_inactive(struct vnode* node);

static inline void vop_lock(struct vnode* node) {
    mutex_acquire(&node->lock, false);
}

static inline void vop_unlock(struct vnode* node) {
    mutex_release(&node->lock);
}

static inline uintmax_t vop_hold(struct vnode* node) {
    return __atomic_add_fetch(&node->refcount, 1, __ATOMIC_SEQ_CST);
}

static inline void vop_release(struct vnode** node) {
    if(__atomic_sub_fetch(&(*node)->refcount, 1, __ATOMIC_SEQ_CST) == 0) {
        vfs_inactive(*node);
        *node = nullptr;
    }
}

static inline int vop_lookup(struct vnode* node, const char* name, struct vnode** result, struct cred* cred) {
    return node->ops->lookup(node, name, result, cred);
}

static inline int vop_create(struct vnode* node, const char* name, struct vattr* attr, int type, struct vnode** result, struct cred* cred) {
    return node->ops->create(node, name, attr, type, result, cred);
}

static inline void vop_init(struct vnode* node, struct vops* vops, enum vfflags flags, enum vtype type, struct vfs* vfs) {
    node->ops = vops;

    mutex_init(&node->lock);
    node->refcount = 1;
    node->flags = flags;
    node->type = type;
    node->vfs = vfs;
    node->vfsmounted = nullptr;
}

static inline int vop_setattr(struct vnode* node, struct vattr* attr, struct cred* cred) {
    return node->ops->setattr(node, attr, cred);
}

static inline int vfs_get_root(struct vfs* vfs, struct vnode** r) {
    return vfs->ops->root(vfs, r);
}

#endif /* _AMETHYST_FILESYSTEM_VIRTUAL_H */

