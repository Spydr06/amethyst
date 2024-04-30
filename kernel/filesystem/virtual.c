#include <filesystem/virtual.h>
#include <mem/heap.h>
#include <sys/spinlock.h>

#include <kernelio.h>
#include <assert.h>

struct vnode* vfs_root = nullptr;

spinlock_t list_lock;
struct vfs* vfs_list;

void vfs_init(void) {
    vfs_root = kmalloc(sizeof(struct vnode));
    assert(vfs_root);

    spinlock_init(list_lock);

    vfs_root->type = V_TYPE_DIR;
    vfs_root->refcount = 1;
}

/*size_t vfs_read(struct fs_node* node, size_t offset, size_t size, uint8_t* buffer) {
    if(node->read)
        return node->read(node, offset, size, buffer);
    return 0;
}

size_t vfs_write(struct fs_node* node, size_t offset, size_t size, const uint8_t* buffer) {
    if(node->write)
        return node->write(node, offset, size, buffer);
    return 0;
}

void vfs_open(struct fs_node* node, uint8_t read, uint8_t write) {
    // TODO: check readable/writable
    if(node->open)
        node->open(node);
}

void vfs_close(struct fs_node* node) {
    if(node->close)
        node->close(node);
}

struct dirent* vfs_readdir(struct fs_node* node, size_t index) {
    if((node->flags & 0x7) == FS_DIRECTORY && node->readdir)
        return node->readdir(node, index);
    return nullptr;
}

struct fs_node* vfs_finddir(struct fs_node* node, const char* name) {
    if((node->flags & 0x7) == FS_DIRECTORY && node->finddir)
        return node->finddir(node, name);
    return nullptr;
}*/

