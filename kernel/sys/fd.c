#include <sys/fd.h>

#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/thread.h>
#include <cpu/cpu.h>
#include <mem/slab.h>

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

struct file* fd_get(size_t fd) {
    struct proc* proc = _cpu()->thread->proc;

    mutex_acquire(&proc->fd_mutex, false);

    struct file* file = fd < proc->fd_count ? proc->fd[fd].file : nullptr;
    if(file)
        fd_hold(file);


    mutex_release(&proc->fd_mutex);
    return file;
}

void fd_free(struct file* file) {
    assert(file);

    if(file->vnode) {
        vfs_close(file->vnode, file_to_vnode_flags(file->flags));
        vop_release(&file->vnode);
    }

    slab_free(file_cache, file);
}

