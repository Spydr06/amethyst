#include <filesystem/temporary.h>

#include <mem/heap.h>
#include <mem/slab.h>

#include <assert.h>

static struct vfsops ops;
static struct scache* node_cache;

void tmpfs_init(void) {
    assert(vfs_register(&ops, "tmpfs") == 0);
    node_cache = slab_newcache(sizeof(struct tmpfs_node), 0, nullptr, nullptr);
    assert(node_cache);
} 

