#include <filesystem/temporary.h>

#include <mem/heap.h>
#include <mem/slab.h>

#include <assert.h>
#include <errno.h>

static int tmpfs_mount(struct vfs** vfs, struct vnode* mount_point, struct vnode* backing, void* data);
static int tmpfs_root(struct vfs* vfs, struct vnode** node);

static struct vfsops vfsops = {
    .mount = tmpfs_mount,
    .root = tmpfs_root
};

static struct vops vops = {

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

