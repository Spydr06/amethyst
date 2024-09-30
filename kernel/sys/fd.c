#include <sys/fd.h>
#include <mem/slab.h>

#include <sys/mutex.h>

#include <assert.h>

struct scache* file_cache = nullptr;

static void file_ctor(struct scache* __unused, void* obj) {
    struct file* file = obj;
    file->vnode = nullptr;
    mutex_init(&file->mutex); 
    file->ref_count = 1;
    file->offset = 0;
}

struct file* fd_allocate(void) {
    if(!file_cache) {
        file_cache = slab_newcache(sizeof(struct file), 0, file_ctor, file_ctor);
        assert(file_cache);
    }

    return slab_alloc(file_cache);
}
