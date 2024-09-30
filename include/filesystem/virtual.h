#ifndef _AMETHYST_FILESYSTEM_VIRTUAL_H
#define _AMETHYST_FILESYSTEM_VIRTUAL_H

#include <abi.h>
#include <sys/mutex.h>

#include <stdint.h>
#include <stddef.h>

#define MAX_FILENAME_LENGTH 256

#define PATH_SEPARATOR '/'

enum vflags {
    V_FLAGS_ROOT = 1
};

enum vfflags {
    V_FFLAGS_READ           = 1,
    V_FFLAGS_WRITE          = 2,
    V_FFLAGS_NONBLOCKING    = 4,
    V_FFLAGS_SHARED         = 8,
    V_FFLAGS_EXEC           = 16,
    V_FFLAGS_NOCTTY         = 32,
    V_FFLAGS_NOCACHE        = 64,
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

enum vattr_bitflags {
    V_ATTR_MODE  = 1,
    V_ATTR_UID   = 2,
    V_ATTR_GID   = 4,
    V_ATTR_ATIME = 8,
    V_ATTR_MTIME = 16,
    V_ATTR_CTIME = 32,

    V_ATTR_ALL = V_ATTR_MODE | V_ATTR_UID | V_ATTR_GID | V_ATTR_ATIME | V_ATTR_MTIME | V_ATTR_CTIME
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
    int dev_major;
    int dev_minor;
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
    mutex_t size_lock;
    int refcount;
    enum vflags flags;
    enum vtype type;
    struct vfs* vfs;
    struct vfs* vfsmounted;
    void* socketbinding;
    struct page* pages;
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
	int (*setattr)(struct vnode *node, struct vattr *attr, int which, struct cred *cred);
	int (*poll)(struct vnode *node, struct polldata *, int events);
	int (*access)(struct vnode *node, mode_t mode, struct cred *cred);
	int (*unlink)(struct vnode *node, const char *name, struct cred *cred);
	int (*link)(struct vnode *node, struct vnode *dir, const char *name, struct cred *cred);
	int (*symlink)(struct vnode *parent, const char *name, struct vattr *attr, const char *path, struct cred *cred);
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
    int (*getpage)(struct vnode* node, uintmax_t offset, struct page* page);
    int (*putpage)(struct vnode* node, uintmax_t offset, struct page* page);
    int (*sync)(struct vnode* node);
} vops_t;

enum vfs_lookup_flags {
    VFS_LOOKUP_PARENT = 0x20000000,
    VFS_LOOKUP_NOLINK = 0x40000000,
    VFS_LOOKUP_INTERNAL = 0x80000000
};

extern struct vnode* vfs_root;

int vfs_lookup(struct vnode** dest, struct vnode* src, const char* path, char* last_comp, enum vfs_lookup_flags flags);

void vfs_init(void);

int vfs_mount(struct vnode* backing, struct vnode* path_ref, const char* path, const char* fs_name, void* data);

int vfs_create(struct vnode* ref, const char* path, struct vattr* attr, enum vtype type, struct vnode** node);
int vfs_register(struct vfsops* vfsops, const char* name);

int vfs_open(struct vnode* ref, const char* path, int flags, struct vnode** res);
int vfs_close(struct vnode* node, int flags);

int vfs_getattr(struct vnode* node, struct vattr* attr);
int vfs_setattr(struct vnode* node, struct vattr* attr, int which);

int vfs_write(struct vnode* node, void* buffer, size_t size, uintmax_t offset, size_t* written, int flags);
int vfs_read(struct vnode* node, void* buffer, size_t size, uintmax_t offset, size_t* read, int flags);

int vfs_link(struct vnode* dest_ref, const char* dest_path, struct vnode* link_ref, const char* link_path, enum vtype type, struct vattr* attr);

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

static inline int vop_open(struct vnode** dest, int flags, struct cred* cred) {
    return (*dest)->ops->open(dest, flags, cred);
}

static inline int vop_close(struct vnode* node, int flags, struct cred* cred) {
    return node->ops->close(node, flags, cred);
}

static inline int vop_setattr(struct vnode* node, struct vattr* attr, int which, struct cred* cred) {
    return node->ops->setattr(node, attr, which, cred);
}

static inline int vop_getattr(struct vnode *node, struct vattr *attr, struct cred *cred) {
    return node->ops->getattr(node, attr, cred);
}

static inline int vop_write(struct vnode *node, void *buffer, size_t size, uintmax_t offset, int flags, size_t *bytes_written, struct cred *cred) {
    return node->ops->write(node, buffer, size, offset, flags, bytes_written, cred);
}

static inline int vop_read(struct vnode* node, void* buffer, size_t size, uintmax_t offset, int flags, size_t* bytes_read, struct cred* cred) {
    return node->ops->read(node, buffer, size, offset, flags, bytes_read, cred);
}

static inline int vop_link(struct vnode* node, struct vnode* dir, const char* name, struct cred* cred) {
    return node->ops->link(node, dir, name, cred);
}

static inline int vop_symlink(struct vnode* node, const char* name, struct vattr* attr, const char* path, struct cred* cred) {
    return node->ops->symlink(node, name, attr, path, cred);
}

static inline void vop_init(struct vnode* node, struct vops* vops, enum vflags flags, enum vtype type, struct vfs* vfs) {
    node->ops = vops;

    mutex_init(&node->lock);
    mutex_init(&node->size_lock);
    node->refcount = 1;
    node->flags = flags;
    node->type = type;
    node->vfs = vfs;
    node->vfsmounted = nullptr;
}

static inline int vop_resize(struct vnode *node, size_t newsize, struct cred *cred) {
    return node->ops->resize(node, newsize, cred);
}

static inline int vop_getpage(struct vnode* node, uintmax_t offset, struct page* page) {
    return node->ops->getpage(node, offset, page);
}

static inline int vop_putpage(struct vnode* node, uintmax_t offset, struct page* page) {
    return node->ops->putpage(node, offset, page);
}
 
static inline int vfs_get_root(struct vfs* vfs, struct vnode** r) {
    return vfs->ops->root(vfs, r);
}

#endif /* _AMETHYST_FILESYSTEM_VIRTUAL_H */

