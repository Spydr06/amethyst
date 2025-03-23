#ifndef _AMETHYST_SYS_FD_H
#define _AMETHYST_SYS_FD_H

#include <filesystem/virtual.h>

#include <sys/mutex.h>
#include <cdefs.h>

#define FDTABLE_LIMIT 256

enum file_flags {
    FILE_READ = 1,
    FILE_WRITE = 2
};

enum o_flags {
    O_RDONLY   = 00,
    O_WRONLY   = 01,
    O_RDWR     = 02,

    O_CREAT    = 0100,
    O_EXCL     = 0200,
    O_NOCTTY   = 0400,
    O_TRUNC    = 01000,
    O_APPEND   = 02000,
    O_NONBLOCK = 04000,
    // ...
    O_DIRECTORY = 0100000,
    O_CLOEXEC   = 02000000,
    // ...
};

struct file {
    mutex_t mutex;
    struct vnode* vnode;
    
    uintmax_t offset;
    int ref_count;
    mode_t mode;

    int flags;
};

struct fd {
    struct file* file;
    int flags;
};

struct proc;

struct file* fd_allocate(void);
void fd_free(struct file* file);
int fd_clone(struct proc* dest);

struct file* fd_get(size_t fd);
int fd_new(int flags, struct file** file, int* fd);
int fd_close(int fd);

static inline void fd_hold(struct file* file) {
    __atomic_add_fetch(&file->ref_count, 1, __ATOMIC_SEQ_CST);
}

static inline void fd_release(struct file* file) {
    if(__atomic_sub_fetch(&file->ref_count, 1, __ATOMIC_SEQ_CST) == 0)
        fd_free(file);
}

static inline enum vfflags file_to_vnode_flags(int flags) {
    enum vfflags vfflags = 0;

    if(flags & FILE_READ)
        vfflags |= V_FFLAGS_READ;
    if(flags & FILE_WRITE)
        vfflags |= V_FFLAGS_WRITE;
    if(flags & O_NONBLOCK)
        vfflags |= V_FFLAGS_NONBLOCKING;
    if(flags & O_NOCTTY)
        vfflags |= V_FFLAGS_NOCTTY;
    // TODO: no caching flags (O_SYNC, ...)

    return vfflags;
}

#endif /* _AMETHYST_SYS_FD_H */

