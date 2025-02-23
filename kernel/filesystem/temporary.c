#include "kernelio.h"
#include <filesystem/temporary.h>
#include <filesystem/virtual.h>
#include <filesystem/device.h>

#include <sys/timekeeper.h>
#include <mem/heap.h>
#include <mem/slab.h>
#include <mem/vmm.h>
#include <mem/page.h>

#include <math.h>
#include <assert.h>
#include <errno.h>

static int tmpfs_mount(struct vfs** vfs, struct vnode* mount_point, struct vnode* backing, void* data);
static int tmpfs_unmount(struct vfs* vfs);
static int tmpfs_root(struct vfs* vfs, struct vnode** node);

static int tmpfs_create(struct vnode* parent, const char* name, struct vattr* attr, int type, struct vnode** result, struct cred* cred);

static int tmpfs_open(struct vnode** nodep, int flags, struct cred* cred);
static int tmpfs_close(struct vnode* node, int flags, struct cred* cred);

static int tmpfs_getattr(struct vnode* node, struct vattr* attr, struct cred* cred);
static int tmpfs_setattr(struct vnode* node, struct vattr* attr, int which, struct cred* cred);
static int tmpfs_resize(struct vnode* node, size_t size, struct cred* cred);
static int tmpfs_getpage(struct vnode* node, uintmax_t offset, struct page* page);
static int tmpfs_putpage(struct vnode* node, uintmax_t offset, struct page* page);

static int tmpfs_mmap(struct vnode *node, void *addr, uintmax_t offset, int flags, struct cred* cred);

static int tmpfs_lookup(struct vnode* parent, const char* name, struct vnode** result, struct cred* cred);

static struct vfsops vfsops = {
    .mount = tmpfs_mount,
    .unmount = tmpfs_unmount,
    .root = tmpfs_root,
};

static struct vops vops = {
    .create = tmpfs_create,
    .open = tmpfs_open,
    .close = tmpfs_close,
    .getattr = tmpfs_getattr,
    .setattr = tmpfs_setattr,
    .resize = tmpfs_resize,
    .getpage = tmpfs_getpage,
    .putpage = tmpfs_putpage,
    .lookup = tmpfs_lookup,
    .mmap = tmpfs_mmap,
};

static struct scache* node_cache;

static uintmax_t id_counter = 0;

void tmpfs_init(void) {
    assert(vfs_register(&vfsops, "tmpfs") == 0);
    node_cache = slab_newcache(sizeof(struct tmpfs_node), 0, nullptr, nullptr);
    assert(node_cache);
} 

static struct tmpfs_node* tmpfs_node_new(struct vfs* vfs, enum vtype type) {
    struct tmpfs_node* node = slab_alloc(node_cache);
    if(!node)
        return nullptr;

    memset(node, 0, sizeof(struct tmpfs_node));

    if(type == V_TYPE_DIR) {
        int err;
        if((err = hashtable_init(&node->children, 32))) {
            errno = err;
            slab_free(node_cache, node);
            return nullptr;
        }

        if((err = hashtable_set(&node->children, node, ".", 1, true))) {
            errno = err;
            hashtable_destroy(&node->children);
            slab_free(node_cache, node);
            return nullptr;
        }
    }

    node->vattr.type = type;
    node->vattr.inode = ++((struct tmpfs*) vfs)->inode_num;
    vop_init(&node->vnode, &vops, 0, type, vfs);

    return node;
}

static int tmpfs_mount(struct vfs** vfs, struct vnode* mount_point __unused, struct vnode* backing __unused, void* data __unused) {
    struct tmpfs* fs = kmalloc(sizeof(struct tmpfs));
    if(!fs)
        return ENOMEM;

    *vfs = (struct vfs*) fs;
    fs->vfs.ops = &vfsops;
    fs->id = __atomic_fetch_add(&id_counter, 1, __ATOMIC_SEQ_CST);

    return 0;
};

static int tmpfs_unmount(struct vfs* vfs) {
    if(!vfs)
        return EINVAL;

    struct tmpfs* fs = (struct tmpfs*) vfs;
    kfree(fs);
    
    if(fs->vfs.root)
        vop_release(&fs->vfs.root);

    return 0;
}

static int tmpfs_root(struct vfs* vfs, struct vnode** node) {
    if(vfs->root) {
        *node = vfs->root;
        return 0;
    }    

    // create root node if not exist
    struct tmpfs_node* tmpnode = tmpfs_node_new(vfs, V_TYPE_DIR);
    if(!node)
        return errno;

    *node = (struct vnode*) tmpnode;
    vfs->root = *node;

    tmpnode->vnode.flags |= V_FLAGS_ROOT;

    return 0;
}

static int tmpfs_create(struct vnode* parent, const char* name, struct vattr* attr, int type, struct vnode** result, struct cred* cred) {
    if(parent->type != V_TYPE_DIR)
        return ENOTDIR;

    struct tmpfs_node* parent_tmpnode= (struct tmpfs_node*) parent;
    size_t name_len = strlen(name);

    void* v;

    vop_lock(parent);
    if(hashtable_get(&parent_tmpnode->children, &v, name, name_len) == 0) {
        vop_unlock(parent);
        return EEXIST;
    }

    struct tmpfs_node* tmpnode = tmpfs_node_new(parent->vfs, type);
    if(!tmpnode) {
        vop_release(&parent);
        return ENOMEM;
    }

    struct vnode* node = (struct vnode*) tmpnode;

    struct timespec time = timekeeper_time();

    struct vattr tmpattr = *attr;

    tmpattr.atime = time;
    tmpattr.ctime = time;
    tmpattr.mtime = time;
    tmpattr.size = 0;
    tmpattr.type = type;

    int err = tmpfs_setattr(node, &tmpattr, V_ATTR_ALL, cred);
    tmpnode->vattr.nlinks = 1;

    if(err) {
        vop_unlock(parent);
        vop_release(&node);
        return err;
    }

    if(type == V_TYPE_DIR) {
        if((err = hashtable_set(&tmpnode->children, parent_tmpnode, "..", 2, true))) {
            vop_unlock(parent);
            vop_release(&node);
            return err;
        }
    }

    err = hashtable_set(&parent_tmpnode->children, tmpnode, name, name_len, true);
    vop_unlock(parent);

    if(err) {
        vop_release(&node);
        return err;
    }

    if(type == V_TYPE_DIR)
        vop_hold(parent);

    node->vfs = parent->vfs;
    *result = node;
    vop_hold(node);

    return err;
}

static int tmpfs_open(struct vnode** nodep, int flags __unused, struct cred* cred __unused) {
    struct vnode* node = *nodep;
    struct tmpfs_node* tmpnode = (struct tmpfs_node*) node;

    if(node->type == V_TYPE_CHDEV || node->type == V_TYPE_BLKDEV) {
        struct vnode* devnode;
        int err = devfs_getnode(node, tmpnode->vattr.rdev_major, tmpnode->vattr.rdev_minor, &devnode);
        if(err)
            return err;

        vop_release(&node);
        vop_hold(devnode);
        *nodep = devnode;
    }

    return 0;
}

static int tmpfs_close(struct vnode* node __unused, int flags __unused, struct cred* cred __unused) {
    return 0;
}

static int tmpfs_getattr(struct vnode* node, struct vattr* attr, struct cred* cred __unused) {
    struct tmpfs_node* tmpnode = (struct tmpfs_node*) node;
    *attr = tmpnode->vattr;
    attr->blocks_used = ROUND_UP(attr->size, PAGE_SIZE) / PAGE_SIZE;
    attr->dev_major = 0;
    attr->dev_minor = ((struct tmpfs*) node->vfs)->id;
    return 0;
}

static int tmpfs_setattr(struct vnode* node, struct vattr* attr, int which, struct cred* cred __unused) {
    vop_lock(node);

    struct tmpfs_node* tmpnode = (struct tmpfs_node*) node;

    if(which & V_ATTR_MODE)
        tmpnode->vattr.mode = attr->mode;
    if(which & V_ATTR_GID)
        tmpnode->vattr.gid = attr->gid;
    if(which & V_ATTR_UID)
        tmpnode->vattr.uid = attr->uid;
    if(which & V_ATTR_ATIME)
        tmpnode->vattr.atime = attr->atime;
    if(which & V_ATTR_MTIME)
        tmpnode->vattr.mtime = attr->mtime;
    if(which & V_ATTR_CTIME)
        tmpnode->vattr.ctime = attr->ctime;

    vop_unlock(node);
    return 0;
}

static int tmpfs_resize(struct vnode* node, size_t size, struct cred* cred __unused) {
    vop_lock(node);

    struct tmpfs_node* tmpnode = (struct tmpfs_node*) node;
    if(size != tmpnode->vattr.size && tmpnode->vnode.type == V_TYPE_REGULAR) {
        if(tmpnode->vattr.size > size)
            vmm_cache_truncate(node, size);

        tmpnode->vattr.size = size;
    }

    vop_unlock(node);
    return 0;
}

static int tmpfs_getpage(struct vnode* node, uintmax_t offset, struct page* page) {
    int err = 0;

    vop_lock(node);
    struct tmpfs_node* tmpnode = (struct tmpfs_node*) node;
    if(offset >= tmpnode->vattr.size)
        err = ENXIO;

    vop_unlock(node);
    if(err)
        return err;

    void* phy = page_get_physical(page);
    void* phy_hhdm = MAKE_HHDM(phy);

    page_hold(page);
    page->flags |= PAGE_FLAGS_PINNED;
    memset(phy_hhdm, 0, PAGE_SIZE);

    return 0;
}

static int tmpfs_putpage(struct vnode* node, uintmax_t offset, struct page* page) {
    (void) node;
    (void) offset;
    (void) page;
    unimplemented();
    return 0;
}

static int tmpfs_mmap(struct vnode *node, void *addr, uintmax_t offset, int flags, struct cred* cred) {
    (void) node;
    (void) addr;
    (void) offset;
    (void) flags;
    (void) cred;
    unimplemented();
    return 0;
}

static int tmpfs_lookup(struct vnode* parent, const char* name, struct vnode** result, struct cred* cred __unused) {
    struct tmpfs_node* tmpparent = (struct tmpfs_node*) parent;
    struct vnode* child = nullptr;
    if(parent->type != V_TYPE_DIR)
        return ENOTDIR;

    vop_lock(parent);
    int err = hashtable_get(&tmpparent->children, (void**) &child, name, strlen(name));
    if(err) {
        vop_unlock(parent);
        return err;
    }

    vop_hold(child);
    vop_unlock(parent);
    *result = child;

    return 0;
}
