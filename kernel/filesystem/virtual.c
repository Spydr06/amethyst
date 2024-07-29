#include <filesystem/virtual.h>

#include <cpu/cpu.h>
#include <mem/heap.h>
#include <mem/vmm.h>
#include <sys/spinlock.h>
#include <sys/thread.h>
#include <sys/scheduler.h>

#include <kernelio.h>
#include <assert.h>
#include <errno.h>
#include <math.h>
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
        return &_cpu()->thread->proc->cred;
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

static inline void bytes_to_pages(uintptr_t offset, size_t size, uintmax_t* page_offset, size_t* page_count, uintptr_t* start_offset) {
    *page_offset = offset / PAGE_SIZE;
    *start_offset = offset % PAGE_SIZE;
    *page_count = ROUND_UP(offset + size, PAGE_SIZE) / PAGE_SIZE - *page_offset;
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

int vfs_write(struct vnode* node, void* buffer, size_t size, uintmax_t offset, size_t* written, int flags) {
    if(node->type != V_TYPE_REGULAR && node->type != V_TYPE_BLKDEV) {
        // when `node` is a speical file, don't buffer
        return vop_write(node, buffer, size, offset, flags, written, get_cred());
    }
    
    *written = 0;
    if(!size)
        return 0;
    

    // overflow
    if(size + offset < offset) {
        return EINVAL;
    }

    int err = 0;
    mutex_acquire(&node->size_lock, false);
    struct vattr attr;
    if((err = vop_getattr(node, &attr, get_cred())))
        goto leave;

    size_t new_size = size + offset > attr.size ? size + offset : 0;

    if(node->type == V_TYPE_REGULAR && new_size) {
        if((err = vop_resize(node, new_size, get_cred() /* ? */)))
            goto leave;
    }
    else if(node->type == V_TYPE_BLKDEV) {
        // TODO: write to block devices
        unimplemented();
    }

    uintptr_t page_offset, page_count, start_offset;
    bytes_to_pages(offset, size, &page_offset, &page_count, &start_offset);
    
    struct page* page;

    unimplemented();

leave:
    mutex_release(&node->size_lock);
    return err;
}

int vfs_mount(struct vnode* backing, struct vnode* path_ref, const char* path, const char* fs_name, void* data) {
    struct vfsops* ops;
    int err = hashtable_get(&fs_table, (void**) &ops, fs_name, strlen(fs_name));
    if(err)
        return err;

    struct vnode* mount_point;
    err = vfs_lookup(&mount_point, path_ref, path, nullptr, 0);
    if(err)
        return err;

    if(mount_point->type != V_TYPE_DIR) {
        vop_release(&mount_point);
        return ENOTDIR;
    }

    struct vfs* vfs;
    err = ops->mount(&vfs, mount_point, backing, data);
    if(err) {
        vop_release(&mount_point);
        return err;
    }

    spinlock_acquire(&list_lock);

    vfs->next = vfs_list;
    vfs_list = vfs;

    spinlock_release(&list_lock);

    mount_point->vfsmounted = vfs;
    vfs->node_covered = mount_point;

    return 0;
}

int vfs_link(struct vnode* dest_ref, const char* dest_path, struct vnode* link_ref, const char* link_path, enum vtype type, struct vattr* attr) {
    if(type != V_TYPE_LINK && type != V_TYPE_REGULAR)
        return EINVAL;

    char* component = kmalloc(strlen(link_path) + 1);
    struct vnode* parent = nullptr;
    int err = vfs_lookup(&parent, link_ref, link_path, component, VFS_LOOKUP_PARENT);
    if(err)
        goto cleanup;

    if(type == V_TYPE_REGULAR) {
        struct vnode* target_node = nullptr;
        err = vfs_lookup(&target_node, dest_ref, dest_path, nullptr, 0);
        if(err) {
            vop_release(&parent);
            goto cleanup;
        }
        err = vop_link(target_node, parent, component, get_cred());
        vop_release(&target_node);
    }
    else
        err = vop_symlink(parent, component, attr, dest_path, get_cred());

cleanup:
    kfree(component);
    return err;
}
