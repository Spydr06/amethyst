#include <sys/fd.h>

#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/thread.h>
#include <cpu/cpu.h>
#include <mem/slab.h>
#include <mem/heap.h>

#include <assert.h>
#include <errno.h>

struct scache* file_cache = nullptr;

static void file_ctor(struct scache* __unused, void* obj) {
    struct file* file = obj;
    file->vnode = nullptr;
    mutex_init(&file->mutex); 
    file->ref_count = 1;
    file->offset = 0;
}

static int get_free_fd(struct proc* proc, int start) {
    int fd;
    for(fd = start; fd < (int) proc->fd_count && proc->fd[fd].file; fd++);
    return fd == (int) proc->fd_count ? -1 : fd;
}

static int grow_fd_table(struct proc* proc, int new_count) {
    if(new_count > FDTABLE_LIMIT)
        return EMFILE;

    assert((size_t) new_count > proc->fd_count);

    void* new_table = krealloc(proc->fd, sizeof(struct fd) * new_count);
    if(!new_table)
        return ENOMEM;

    proc->fd = new_table;
    proc->fd_count = new_count;

    return 0;
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

int fd_new(int flags, struct file** filep, int* fdp) {
    struct file* file = fd_allocate();
    if(!file)
        return ENOMEM;

    struct proc* proc = _cpu()->thread->proc;
    mutex_acquire(&proc->fd_mutex, false);

    int fd = get_free_fd(proc, proc->fd_first);
    if(fd < 0) {
        fd = (int) proc->fd_count;
        int err = grow_fd_table(proc, fd + 1);
        if(err) {
            mutex_release(&proc->fd_mutex);
            fd_free(file);
            return err;
        }
    }

    proc->fd_first = fd + 1;
    proc->fd[fd].file = file;
    proc->fd[fd].flags = flags;

    mutex_release(&proc->fd_mutex);

    *filep = file;
    *fdp = fd;

    return 0;
}

int fd_close(int fd) {
    struct proc* proc = _cpu()->thread->proc;
    mutex_acquire(&proc->fd_mutex, false);

    struct file* file = fd < (int) proc->fd_count ? proc->fd[fd].file : nullptr;
    if(file) {
        proc->fd[fd].file = nullptr;
        if((int) proc->fd_first > fd)
            proc->fd_first = fd;
    }

    mutex_release(&proc->fd_mutex);

    if(!file)
        return EBADF;

    fd_release(file);
    return 0;
}

void fd_free(struct file* file) {
    assert(file);

    if(file->vnode) {
        vfs_close(file->vnode, file_to_vnode_flags(file->flags));
        vop_release(&file->vnode);
    }

    slab_free(file_cache, file);
}

int fd_clone(struct proc* dest) {
    struct proc* proc = current_thread()->proc;
    int err = 0;
    mutex_acquire(&proc->fd_mutex, false);

    dest->fd_count = proc->fd_count;
    dest->fd_first = proc->fd_first;
    dest->fd = kmalloc(proc->fd_count * sizeof(struct fd));
    if(!dest->fd) {
        err = ENOMEM;
        goto cleanup;
    }

    for(size_t i = 0; i < dest->fd_count; i++) {
        if(!proc->fd[i].file)
            continue;

        dest->fd[i] = proc->fd[i];
        fd_hold(dest->fd[i].file);
    }

cleanup:
    mutex_release(&proc->fd_mutex);
    return err;
}

