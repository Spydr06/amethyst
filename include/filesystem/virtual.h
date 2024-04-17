#ifndef _AMETHYST_FILESYSTEM_VIRTUAL_H
#define _AMETHYST_FILESYSTEM_VIRTUAL_H

#include <stdint.h>
#include <stddef.h>

#define MAX_FILENAME_LENGTH 256

struct fs_node;
struct dirent;

enum fs_flags : uint32_t {
    FS_FILE = 1,
    FS_DIRECTORY,
    FS_CHARDEV,
    FS_BLOCKDEV,
    FS_PIPE,
    FS_SYMLINK,
    FS_MOUNTPOINT
};

typedef uint32_t gid_t;
typedef uint32_t uid_t;
typedef uint32_t ino_t;

typedef size_t (*read_type_t)(struct fs_node*, size_t, size_t, uint8_t*);
typedef size_t (*write_type_t)(struct fs_node*, size_t, size_t, const uint8_t*);
typedef void (*open_type_t)(struct fs_node*);
typedef void (*close_type_t)(struct fs_node*);
typedef struct dirent* (*readdir_type_t)(struct fs_node*, size_t);
typedef struct fs_node* (*finddir_type_t)(struct fs_node*, const char*);

struct inode {
    ino_t ino;
};

struct fs_node {
    char name[MAX_FILENAME_LENGTH];
    uint32_t mask;
    uid_t uid;
    gid_t gid;
    enum fs_flags flags;    
    struct inode inode;
    size_t length;

    read_type_t read;
    write_type_t write;
    open_type_t open;
    close_type_t close;

    union {
        struct {
            readdir_type_t readdir;
            finddir_type_t finddir;
        };

        struct fs_node* ptr;
    };
};

struct dirent {
    char name[MAX_FILENAME_LENGTH];
    ino_t ino;
};

extern struct fs_node* vfs;

void vfs_init(void);

size_t vfs_read(struct fs_node* node, size_t offset, size_t size, uint8_t* buffer);
size_t vfs_write(struct fs_node* node, size_t offset, size_t size, const uint8_t* buffer);
void vfs_open(struct fs_node* node, uint8_t read, uint8_t write);
void vfs_close(struct fs_node* node);

struct dirent* vfs_readdir(struct fs_node*, size_t index);
struct fs_node* vfs_finddir(struct fs_node*, const char* name);

#endif /* _AMETHYST_FILESYSTEM_VIRTUAL_H */

