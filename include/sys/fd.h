#ifndef _AMETHYST_SYS_FD_H
#define _AMETHYST_SYS_FD_H

#include <filesystem/virtual.h>

#include <sys/mutex.h>

enum file_flags {
    FILE_READ = 1,
    FILE_WRITE = 2
};

struct file {
    mutex_t mutex;
    struct vnode* vnode;
    
    uintmax_t offset;
    int ref_count;
    mode_t mode;

    enum file_flags flags;
};

struct fd {
    struct file* file;
    int flags;
};

struct file* fd_allocate(void);

#endif /* _AMETHYST_SYS_FD_H */

