#include <filesystem/virtual.h>

#include <cpu/cpu.h>
#include <mem/heap.h>
#include <mem/vmm.h>
#include <mem/page.h>
#include <sys/spinlock.h>
#include <sys/mutex.h>
#include <sys/semaphore.h>
#include <sys/thread.h>
#include <sys/proc.h>

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

static struct cred* get_cred(void);

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

int vfs_open(struct vnode* ref, const char* path, int flags, struct vnode** res) {
    struct vnode* tmp = nullptr;
    int err = vfs_lookup(&tmp, ref, path, nullptr, 0);
    if(err)
        return err;

    if((err = vop_open(&tmp, flags, get_cred())))
        vop_release(&tmp);
    else
        *res = tmp;

    return err;
}

int vfs_close(struct vnode* node, int flags) {
    return vop_close(node, flags, get_cred());
}

int vfs_getattr(struct vnode* node, struct vattr* attr) {
    return vop_getattr(node, attr, get_cred());
}

int vfs_setattr(struct vnode* node, struct vattr* attr, int which) {
    return vop_setattr(node, attr, which, get_cred());
}

void vfs_inactive(struct vnode *node) {
    if(node->socketbinding)
        unimplemented();
//        localsock_leavebinding(vnode);
    node->ops->inactive(node);
}

static struct cred* get_cred(void) {
    struct proc* proc = current_proc();
    return proc ? &proc->cred : &kernel_cred;
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
        if(comp_buffer[i] == PATH_SEPARATOR)
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

int vfs_realpath(struct vnode* node, char** dest_path, size_t* dest_size, enum vfs_lookup_flags flags) {
    struct vnode *current;
    int err = highest_node_in_mp(node, &current);
    if(err)
        return err;

    vop_hold(current);

    // check if we are in root
    if(current == node) {
        *dest_size += 1;
        *dest_path = kmalloc(*dest_size * sizeof(char));
        *dest_path[0] = PATH_SEPARATOR;
        *dest_path[1] = '\0';

        vop_release(&current);
        return 0;
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

    if(current->type == V_TYPE_LINK && (flags & VFS_LOOKUP_NOLINK) == 0) {
        // TODO: dereference symlinks
        unimplemented();
    }

    unimplemented();
    vop_release(&current);
    return 0;
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

    mutex_acquire(&node->size_lock, false);
    
    int err = 0;
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

    if(start_offset) {
        err = vmm_cache_get_page(node, page_offset * PAGE_SIZE, &page);
        if(err)
            goto leave;

        size_t write_size = MIN(PAGE_SIZE - start_offset, size);
        void* address = MAKE_HHDM(page_get_physical(page));

        memcpy((void*) ((uintptr_t) address + start_offset), buffer, write_size);
        vmm_cache_make_dirty(page);
        
        *written += write_size;
        page_offset += 1;
        page_count -= 1;

        if(flags & V_FFLAGS_NOCACHE)
            unimplemented();
        
        page_release(page);
    }
    
    for(uintmax_t offset = 0; offset < page_count * PAGE_SIZE; offset += PAGE_SIZE) {
        if((err = vmm_cache_get_page(node, page_offset * PAGE_SIZE + offset, &page)))
            goto leave;
        
        size_t write_size = MIN(PAGE_SIZE, size - *written);
        void* address = MAKE_HHDM(page_get_physical(page));
        memcpy(address, (void*)((uintptr_t) buffer + *written), write_size);

        vmm_cache_make_dirty(page);
        *written += write_size;

        if(flags & V_FFLAGS_NOCACHE)
            unimplemented();

        page_release(page);
    }

leave:
    mutex_release(&node->size_lock);
    return err;
}

int vfs_read(struct vnode* node, void* buffer, size_t size, uintmax_t offset, size_t* bytes_read, int flags) {
    if(node->type != V_TYPE_REGULAR && node->type != V_TYPE_BLKDEV)
        // special file
        return vop_read(node, buffer, size, offset, flags, bytes_read, get_cred());

    int err = 0;
    
    *bytes_read = 0;
    if(!size)
        return 0;

    if(size + offset < offset)
        return EOVERFLOW;    

    mutex_acquire(&node->size_lock, false);
    size_t node_size = 0;

    if(node->type == V_TYPE_REGULAR) {
        struct vattr attr;
        if((err = vop_getattr(node, &attr, get_cred())))
            goto leave;
        
        node_size = attr.size;
    }
    else
        unimplemented();
    
    if(offset >= node_size)
        goto leave;

    size = MIN(offset + size, node_size) - offset;

    uintmax_t page_offset, page_count, start_offset;
    bytes_to_pages(offset, size, &page_offset, &page_count, &start_offset);
    struct page* page = nullptr;

    if(start_offset) {
        // unaligned first page
        if((err = vmm_cache_get_page(node, page_offset * PAGE_SIZE, &page)))
            goto leave;

        size_t read_size = MIN(PAGE_SIZE - start_offset, size);
        void* address = MAKE_HHDM(page_get_physical(page));

        memcpy(buffer, (void*)((uintptr_t) address + start_offset), read_size);
        *bytes_read += read_size;
        page_offset++;
        page_count--;

        if(flags & V_FFLAGS_NOCACHE) {
            // remove page from vmm cache
            unimplemented();
        }

        page_release(page);
    }

    for(uintmax_t offset = 0; offset < page_count * PAGE_SIZE; offset += PAGE_SIZE) {
        // remaining pages
        if((err = vmm_cache_get_page(node, page_offset * PAGE_SIZE + offset, &page)))
            goto leave;

        size_t read_size = MIN(PAGE_SIZE, size - *bytes_read);
        void* address = MAKE_HHDM(page_get_physical(page));

        memcpy((void*)((uintptr_t) buffer + *bytes_read), address, read_size);
        *bytes_read += read_size;

        if(flags & V_FFLAGS_NOCACHE) {
            // remove page from vmm cache
            unimplemented();
        }

        page_release(page);
    }

leave:
    mutex_release(&node->size_lock);
    return err;
}

int vfs_getdents(struct vnode* node, struct amethyst_dirent *buffer, size_t count, uintmax_t offset, size_t *readcount) {
    return vop_getdents(node, buffer, count, offset, readcount);
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
    if(vfs->next)
        vfs->next->prev = vfs;
    vfs->prev = nullptr;
    vfs_list = vfs;

    spinlock_release(&list_lock);

    mount_point->vfsmounted = vfs;
    vfs->node_covered = mount_point;

    return 0;
}

int vfs_umount(struct vnode* path_ref, const char* path) {
    struct vnode* cover_dir;
    int err = vfs_lookup(&cover_dir, path_ref, path, nullptr, 0);
    if(err)
        return err;

    struct vfs* vfs = cover_dir->vfs;
    struct vnode* mount_point = vfs->node_covered;

    if(!vfs || !mount_point || mount_point->vfsmounted != vfs || !vfs->ops->unmount)
        return EINVAL;

    struct vnode* mount_fs_root = nullptr;
    err = vfs->ops->root(vfs, &mount_fs_root);
    if(err)
        return err;

    if(mount_fs_root != cover_dir) // cover_dir is not mount point
        return EINVAL;

    err = vfs->ops->unmount(vfs);
    if(err)
        return err;

    spinlock_acquire(&list_lock);

    if(vfs->next)
        vfs->next->prev = vfs->prev;
    if(vfs->prev)
        vfs->prev->next = vfs->next;

    vfs->prev = nullptr;
    vfs->next = nullptr;

    spinlock_release(&list_lock);

    mount_point->vfsmounted = nullptr;
    vfs->node_covered = nullptr;

    vop_release(&mount_point);

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
