#include "kernelio.h"
#include <filesystem/device.h>
#include <filesystem/virtual.h>

#include <sys/mutex.h>
#include <mem/slab.h>
#include <mem/heap.h>

#include <assert.h>
#include <string.h>
#include <errno.h>
#include <hashtable.h>

static struct scache* node_cache;

static mutex_t table_mutex;
static hashtable_t dev_table;

static uintmax_t current_inode = 0;
static struct dev_node* devfs_root;

static struct vfsops vfsops = {
    .mount = devfs_mount,
    .unmount = devfs_unmount,
    .root = devfs_get_root
};

static struct vops vnode_ops = {
    .create = devfs_create,
    .open = devfs_open,
    .close = devfs_close,
    .read = devfs_read,
    .write = devfs_write,
    .getattr = devfs_getattr,
    .setattr = devfs_setattr,
    .inactive = devfs_inactive,
    .lookup = devfs_lookup,
};

static void ctor(struct scache* cache __unused, void* obj) {
    struct dev_node* node = obj;
    memset(node, 0, sizeof(struct dev_node));
    vop_init(&node->vnode, &vnode_ops, 0, 0, nullptr);
    node->vattr.inode = current_inode++;
}

void devfs_init(void) {
    assert(hashtable_init(&dev_table, 50) == 0);
    node_cache = slab_newcache(sizeof(struct dev_node), 0, ctor, ctor);
    assert(node_cache);
    mutex_init(&table_mutex);
    
    devfs_root = slab_alloc(node_cache);
    assert(devfs_root);
    devfs_root->vnode.type = V_TYPE_DIR;
    devfs_root->vnode.flags = V_FLAGS_ROOT;

    assert(hashtable_init(&devfs_root->children, 100) == 0);
    assert(vfs_register(&vfsops, "devfs") == 0);
    assert(hashtable_set(&devfs_root->children, devfs_root, ".", 1, true) == 0);
}

int devfs_register(struct devops* devops, const char* name, int type, int major, int minor, mode_t mode) {
    assert(type == V_TYPE_CHDEV || type == V_TYPE_BLKDEV);
    struct dev_node* master = slab_alloc(node_cache);
    if(!master)
        return ENOMEM;

    master->devops = devops;
    
    int key[2] = {major, minor};

    mutex_acquire(&table_mutex, false);
    int err = hashtable_set(&dev_table, master, key, sizeof(int[2]), true);
    mutex_release(&table_mutex);

    if(err) {
        slab_free(node_cache, master);
        return err;
    }

    struct vattr attr = {0};
    attr.mode = mode;
    attr.rdev_major = major;
    attr.rdev_minor = minor;

    struct vnode* new_vnode = nullptr;

    err = vfs_create((struct vnode*) devfs_root, name, &attr, type, &new_vnode);
    if(err) {
        mutex_acquire(&table_mutex, false);
        assert(hashtable_remove(&dev_table, key, sizeof(int[2])) == 0);
        mutex_release(&table_mutex);
        slab_free(node_cache, master);
        return err;
    }

    struct dev_node* new_dev_node = (struct dev_node*) new_vnode;
    master->vattr = new_dev_node->vattr;
    master->vnode.type = type;
    new_dev_node->master = master;

    return 0;
}

void devfs_remove(const char* name, int major, int minor) {
    int key[2] = {major, minor};

    // remove dev_table reference
    mutex_acquire(&table_mutex, false);
    assert(hashtable_remove(&dev_table, key, sizeof(int[2])) == 0);
    mutex_release(&table_mutex);

    // get parent
    struct vnode* parent;
    char name_buffer[100];
    assert(vfs_lookup(&parent, (struct vnode*) devfs_root, name, name_buffer, VFS_LOOKUP_PARENT) == 0);
    struct dev_node* parent_dev_node = (struct dev_node*) parent;

    vop_lock(parent);

    // get device vnode
    void* tmp;
    assert(hashtable_get(&parent_dev_node->children, &tmp, name_buffer, strlen(name_buffer)) == 0);
    struct vnode* vnode = tmp;
    struct dev_node* node = tmp;
    assert(node->vattr.rdev_major == major && node->vattr.rdev_minor == minor);
    assert(hashtable_remove(&parent_dev_node->children, name_buffer, strlen(name_buffer)) == 0);

    vop_unlock(parent);

    vop_release(&vnode);
}

int devfs_get_root(struct vfs* vfs, struct vnode** node) {
    devfs_root->vnode.vfs = vfs;
    *node = (struct vnode*) devfs_root;
    return 0;
}

int devfs_lookup(struct vnode* node, const char* name, struct vnode** result, struct cred* __unused) {
    struct dev_node* dev_node = (struct dev_node*) node;
    if(node->type != V_TYPE_DIR)
        return ENOTDIR;

    void* v;
    mutex_acquire(&node->lock, false);

    mutex_acquire(&table_mutex, false);
    int err = hashtable_get(&dev_node->children, &v, name, strlen(name));
    mutex_release(&table_mutex);

    if(err) {
        mutex_release(&node->lock);
        return err;
    }

    struct vnode* rnode = v;
    vop_hold(rnode);

    mutex_release(&node->lock);
    *result = rnode;

    return 0;
}

int devfs_find(const char* name, struct vnode** dest) {
    int err = vfs_lookup(dest, (struct vnode*) devfs_root, name, nullptr, 0);
    if(!err)
        vop_unlock(*dest);
    return err;
}

int devfs_mount(struct vfs** vfs, struct vnode* mount_point __unused, struct vnode* backing __unused, void* data __unused) {
    struct vfs* vfs_ptr = kmalloc(sizeof(struct vfs));
    if(!vfs_ptr)
        return ENOMEM;

    *vfs = vfs_ptr;
    vfs_ptr->ops = &vfsops;

    return 0;
}

int devfs_unmount(struct vfs* vfs) {
    if(!vfs)
        return EINVAL;
    kfree(vfs);
    return 0;
}

int devfs_getattr(struct vnode* node, struct vattr* attr, struct cred* cred) {
    struct dev_node* dev_node = (struct dev_node*) node;

    if(dev_node->physical && dev_node->physical != node)
        return vop_getattr(dev_node->physical, attr, cred);
    
    *attr = dev_node->vattr;
    attr->type =node->type;
    return 0;
}

int devfs_setattr(struct vnode* node, struct vattr* attr, int which, struct cred* cred) {
    struct dev_node* dev_node = (struct dev_node*) node;

    if(dev_node->physical && dev_node->physical != node)
        return vop_setattr(dev_node->physical, attr, which, cred);

    vop_lock(node);
    if(which & V_ATTR_GID)
        dev_node->vattr.gid = attr->gid;
    if(which & V_ATTR_UID)
        dev_node->vattr.uid = attr->uid;
    if(which & V_ATTR_MODE)
        dev_node->vattr.mode = attr->mode;
    if(which & V_ATTR_ATIME)
        dev_node->vattr.atime = attr->atime;
    if(which & V_ATTR_MTIME)
        dev_node->vattr.mtime = attr->mtime;
    if(which & V_ATTR_CTIME)
        dev_node->vattr.ctime = attr->ctime;
    vop_unlock(node);

    return 0;
}

int devfs_create(struct vnode* parent, const char* name, struct vattr* attr, int type, struct vnode** result, struct cred* cred) {
    if(type != V_TYPE_CHDEV && type != V_TYPE_BLKDEV && type != V_TYPE_DIR)
        return EINVAL;

    struct dev_node* parent_dev_node = (struct dev_node*) parent;

    size_t name_len = strlen(name);
    void* v;
    vop_lock(parent);
    if(hashtable_get(&parent_dev_node->children, &v, name, name_len) == 0) {
        vop_unlock(parent);
        return EEXIST;
    }

    struct dev_node* node = slab_alloc(node_cache);
    if(!node) {
        vop_unlock(parent);
        return ENOMEM;
    }

    struct vattr tmp_attr = *attr;
// TODO: struct timespec time; 
// tmp_attr.atime = tmp_attr.ctime = tmp_attr.mtime = time;
    tmp_attr.size = 0;
    assert(devfs_setattr(&node->vnode, &tmp_attr, V_ATTR_ALL, cred) == 0);
    node->vattr.nlinks = 0;
    node->vattr.rdev_major = tmp_attr.rdev_major;
    node->vattr.rdev_minor = tmp_attr.rdev_minor;
    node->physical = &node->vnode;
    node->vnode.vfs = parent->vfs;
    node->vnode.type = type;

    int err;

    if(type == V_TYPE_DIR) {
        err = hashtable_init(&node->children, 100);
        if(err) {
            slab_free(node_cache, node);
            vop_unlock(parent);
            return err;
        }

        if(hashtable_set(&node->children, node, ".", 1, true) || hashtable_set(&node->children, parent, "..", 2, true)) {
            hashtable_destroy(&node->children);
            slab_free(node_cache, node);
            vop_unlock(parent);
            return err;
        }
    }

    err = hashtable_set(&parent_dev_node->children, node, name, name_len, true);
    if(err) {
        if(type == V_TYPE_DIR)
            hashtable_destroy(&node->children);
        vop_unlock(parent);
        slab_free(node_cache, node);
        return err;
    }

    if(type == V_TYPE_DIR)
        vop_hold(parent);

    vop_hold(&node->vnode);
    vop_unlock(parent);
    *result = &node->vnode;

    return 0;
}

int devfs_open(struct vnode** nodep, int flags, struct cred* __unused) {
    struct dev_node* dev_node = (struct dev_node*) *nodep;

    if((*nodep)->type != V_TYPE_CHDEV && (*nodep)->type != V_TYPE_BLKDEV)
        return 0;

    if(dev_node->master)
        dev_node = dev_node->master;

    if(dev_node->devops->open == nullptr)
        return 0;

    return dev_node->devops->open(dev_node->vattr.rdev_minor, nodep, flags);
}

int devfs_close(struct vnode* node, int flags, struct cred* __unused) {
    struct dev_node* dev_node = (struct dev_node*) node;

    if(node->type != V_TYPE_CHDEV && node->type != V_TYPE_BLKDEV)
        return 0;

    if(dev_node->master)
        dev_node = dev_node->master;

    if(!dev_node->devops->close)
        return 0;

    return dev_node->devops->close(dev_node->vattr.rdev_minor, flags);
}

int devfs_read(struct vnode* node, void* buffer, size_t size, uintmax_t offset, int flags, size_t* bytes_read, struct cred* __unused) {
    if(node->type == V_TYPE_DIR)
        return EISDIR;

    if(node->type != V_TYPE_BLKDEV && node->type != V_TYPE_CHDEV)
        return ENODEV;

    struct dev_node* dev_node = (struct dev_node*) node;
    if(dev_node->master)
        dev_node = dev_node->master;

    if(dev_node->devops->read == NULL)
        return ENODEV;

    return dev_node->devops->read(dev_node->vattr.rdev_minor, buffer, size, offset, flags, bytes_read);
}

int devfs_write(struct vnode* node, void* buffer, size_t size, uintmax_t offset, int flags, size_t* bytes_written, struct cred* __unused) {
    struct dev_node* dev_node = (struct dev_node*) node;
    if(dev_node->master)
        dev_node = dev_node->master;

    if(!dev_node->devops->write)
        return ENODEV;

    return dev_node->devops->write(dev_node->vattr.rdev_minor, buffer, size, offset, flags, bytes_written);
}

int devfs_getnode(struct vnode* physical, int major, int minor, struct vnode** node) {
    int key[2] = {major, minor};

    mutex_acquire(&table_mutex, false);
    void* r;
    int err = hashtable_get(&dev_table, &r, key, sizeof(int[2]));
    mutex_release(&table_mutex);

    if(err)
        return err == ENOENT ? ENXIO : err;

    struct dev_node* new_node = slab_alloc(node_cache);
    if(!new_node)
        return ENOMEM;

    struct dev_node* master = r;
    new_node->master = master;
    vop_hold(&master->vnode);

    if(physical) {
        new_node->physical = physical;
        vop_hold(physical);
    }

    *node = &new_node->vnode;
    new_node->vnode.type = master->vnode.type;
    return 0;
}

int devfs_inactive(struct vnode* node) {
    vop_lock(node);

    struct dev_node* dev_node = (struct dev_node*) node;
    struct vnode* master = (struct vnode*) dev_node->master;

    if(dev_node->physical && dev_node->physical != node)
        vop_release(&dev_node->physical);
    if(dev_node->master)
        vop_release(&master);

    if(dev_node->devops && dev_node->devops->inactive)
        dev_node->devops->inactive(dev_node->vattr.rdev_minor);

    slab_free(node_cache, node);
    return 0;
}
