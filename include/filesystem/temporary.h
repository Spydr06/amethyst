#ifndef _AMETHYST_FILESYSTEM_TEMPORARY_H
#define _AMETHYST_FILESYSTEM_TEMPORARY_H

#include <filesystem/virtual.h>
#include <hashtable.h>

struct tmpfs {
    struct vfs vfs;
    uintmax_t inode_num;
    uintmax_t id;
};

struct tmpfs_node {
    struct vnode vnode;
    struct vattr vattr;
    union {
        hashtable_t children;
        struct {
            void* data;
            size_t page_count;
        };
        char* link;
    };
};

void tmpfs_init(void);

#endif /* _AMETHYST_FILESYSTEM_TEMPORARY_H */

