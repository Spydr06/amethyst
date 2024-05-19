#include <filesystem/virtual.h>

#include <cpu/cpu.h>
#include <mem/heap.h>
#include <sys/spinlock.h>

#include <kernelio.h>
#include <assert.h>
#include <errno.h>
#include <hashtable.h>

#define MAX_LINK_DEPTH 64
#define MAX_PATH_NAME 512

struct vnode* vfs_root = nullptr;

spinlock_t list_lock;
struct vfs* vfs_list;

static hashtable_t fs_table;

static struct cred kernel_cred = {
    .gid = 0,
    .uid = 0
};

void vfs_init(void) {
    assert(hashtable_init(&fs_table, 20) == 0);

    vfs_root = kmalloc(sizeof(struct vnode));
    assert(vfs_root);

    spinlock_init(list_lock);

    vfs_root->type = V_TYPE_DIR;
    vfs_root->refcount = 1;
}

int vfs_register(struct vfsops* ops, const char* name) {
    return hashtable_set(&fs_table, ops, name, strlen(name), true);
}

void vfs_inactive(struct vnode *node) {
    if(node->socketbinding)
        unimplemented();
//        localsock_leavebinding(vnode);
    node->ops->inactive(node);
}

static struct cred* get_cred(void) {
    if(!_cpu()->thread /* || !_cpu()->thread->proc */)
        return &kernel_cred;
    else
//        return &_cpu()->thread->proc->cred;
        unimplemented();
}

static int highest_node_in_mp(struct vnode* node, struct vnode** dst) {
    int err = 0;
    while(!err && node->vfsmounted)
        err = vfs_get_root(node->vfsmounted, &node);
    *dst = node;
    return err;
}

static struct vnode* lowest_node_in_mp(struct vnode* node) {
    while(node->flags & V_FLAGS_ROOT)
        node = node->vfs->node_covered;
    return node;
}

int vfs_lookup(struct vnode** dest, struct vnode* src, const char* path, char* last_comp, enum vfs_lookup_flags flags) {
    if((flags & VFS_LOOKUP_INTERNAL) && (flags & 0xff) > MAX_LINK_DEPTH)
        return ELOOP;

    size_t path_len = strlen(path);
    if(!path_len) {
        if(flags & VFS_LOOKUP_PARENT)
            return ENOENT;

        struct vnode* result = src;
        int err = highest_node_in_mp(src, &result);
        if(err)
            return err;

        vop_hold(result);
        *dest = result;
        return 0;
    }
    else if(path_len > MAX_PATH_NAME)
        return ENAMETOOLONG;

    struct vnode* current = src;
    int err = highest_node_in_mp(src, &current);
    if(err)
        return err;

    char* comp_buffer = kmalloc(path_len + 1);
    if(!comp_buffer)
        return ENOMEM;

    strcpy(comp_buffer, path);

    for(size_t i = 0; i < path_len; i++) {
        if(comp_buffer[i] == '/')
            comp_buffer[i] = '\0';
    }

    struct vnode* next;
    err = 0;
    vop_hold(current);

    for(size_t i = 0; i < path_len; i++) {
        if(comp_buffer[i] == '\0')
            continue;

        if(current->type != V_TYPE_DIR) {
            err = ENOTDIR;
            break;
        }

        char* component = &comp_buffer[i];
        size_t comp_len = strlen(component);
        bool is_last = i + comp_len == path_len;

        if(!is_last) {
            size_t j;
            for(j = i + comp_len; j < path_len && comp_buffer[j] == '\0'; j++);
            is_last = j == path_len;
        }

        if(is_last && (flags & VFS_LOOKUP_PARENT)) {
            assert(last_comp);
            strcpy(last_comp, component);
            break;
        }

        if(strcmp(component, "..") == 0) {
            // if we are already at the root, skip to the next component
            struct vnode* root = nullptr;
            assert(highest_node_in_mp(vfs_root, &root) == 0);
            if(root == current) {
                i += comp_len;
                continue;
            }
            
            // if the root is of a mounted fs, go to it
            if(current->flags & V_FLAGS_ROOT) {
                struct vnode* next = lowest_node_in_mp(current);
                if(next != current) {
                    vop_hold(next);
                    vop_release(&current);
                    current = next;
                }
            }
        }

        err = vop_lookup(current, component, &next, get_cred());
        if(err)
            break;

        struct vnode* result = next;
        err = highest_node_in_mp(next, &result);
        if(err) {
            vop_release(&next);
            break;
        }

        if(result != next) {
            vop_hold(result);
            vop_release(&next);
            next = result;
        }

        if(next->type == V_TYPE_LINK && (!is_last || (is_last && (flags & VFS_LOOKUP_NOLINK) == 0))) {
            // TODO: dereference symlinks
            unimplemented();
        }

        vop_release(&current);
        current = next;
        i += comp_len;
    }

    if(err)
        vop_release(&current);
    else
        *dest = current;

    kfree(comp_buffer);
    return err;
}

int vfs_create(struct vnode* ref, const char* path, struct vattr* attr, enum vtype type, struct vnode** node) {
    struct vnode* parent;
    char* component = kmalloc(strlen(path) + 1);
    int err = vfs_lookup(&parent, ref, path, component, VFS_LOOKUP_PARENT);
    if(err)
        goto cleanup;

    struct vnode* ret;
    err = vop_create(parent, component, attr, type, &ret, get_cred());
    vop_release(&parent);
    if(err)
        goto cleanup;

    if(node)
        *node = ret;
    else
        vop_release(&ret);

cleanup:
    kfree(component);
    return err;
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

