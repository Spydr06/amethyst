#include <filesystem/device.h>
#include <filesystem/virtual.h>

#include <sys/mutex.h>
#include <mem/slab.h>
#include <mem/heap.h>

#include <assert.h>
#include <string.h>
#include <errno.h>
#include <hashtable.h>
#include <time.h>

static struct scache* node_cache;

static mutex_t table_mutex;
static hashtable_t dev_table;

static uintmax_t current_inode = 0;
static struct dev_node* devfs_root;

static struct vfsops vfsops = {
    .mount = devfs_mount,
    .root = devfs_get_root
};

static struct vops vnode_ops = {
    .create = devfs_create,
    .setattr = devfs_setattr
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

int devfs_mount(struct vfs** vfs, struct vnode* mount_point __unused, struct vnode* backing __unused, void* data __unused) {
    struct vfs* vfs_ptr = kmalloc(sizeof(struct vfs));
    if(!vfs_ptr)
        return ENOMEM;

    *vfs = vfs_ptr;
    vfs_ptr->ops = &vfsops;

    return 0;
}

int devfs_setattr(struct vnode* node, struct vattr* attr, struct cred* cred) {
    struct dev_node* dev_node = (struct dev_node*) node;

    if(dev_node->physical && dev_node->physical != node)
        return vop_setattr(dev_node->physical, attr, cred);

    vop_lock(node);
    dev_node->vattr.gid = attr->gid;
    dev_node->vattr.uid = attr->uid;
    dev_node->vattr.mode = attr->mode;
    dev_node->vattr.atime = attr->atime;
    dev_node->vattr.mtime = attr->mtime;
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
    assert(devfs_setattr(&node->vnode, &tmp_attr, cred) == 0);
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

