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
    struct proc* proc = current_proc();
    assert(proc);

    mutex_acquire(&proc->fd_mutex);

    struct file* file = fd < proc->fd_count ? proc->fd[fd].file : nullptr;
    if(file)
        fd_hold(file);

    mutex_release(&proc->fd_mutex);
    return file;
}

int fd_reserve(void) {
    struct proc* proc = current_proc();
    assert(proc);

    mutex_acquire(&proc->fd_mutex);

    int fd = get_free_fd(proc, proc->fd_first);
    if(fd < 0) {
        fd = (int) proc->fd_count;
        int err = grow_fd_table(proc, fd + 1);
        if(err) {
            mutex_release(&proc->fd_mutex);
            return -err;
        }
    }

    proc->fd_first = fd + 1;
    proc->fd[fd].file = NULL;
    proc->fd[fd].flags = 0;

    mutex_release(&proc->fd_mutex);
    return fd;
}

int fd_exact(int fd) {
    struct proc* proc = current_proc();
    assert(proc);

    int err = 0;
    mutex_acquire(&proc->fd_mutex);

    if(fd < (int) proc->fd_count) {
        if(proc->fd[fd].file)
            fd_release(proc->fd[fd].file);

        proc->fd[fd].file = nullptr;
        proc->fd[fd].flags = 0;

        goto cleanup;
    }

    if((err = grow_fd_table(proc, fd + 1)))
        goto cleanup;

    proc->fd[fd].file = NULL;
    proc->fd[fd].flags = 0;

cleanup:
    mutex_release(&proc->fd_mutex);
    return err;
}

void fd_vacate(int fd) {
    struct proc* proc = current_proc();
    assert(proc);

    mutex_acquire(&proc->fd_mutex);

    if(fd < (int) proc->fd_count) {
        assert(proc->fd[fd].file == nullptr);
        if((int) proc->fd_first > fd)
            proc->fd_first = fd;
    }

    mutex_release(&proc->fd_mutex);
}

int fd_new(int flags, struct file** filep, int* fdp) {
    struct file* file = fd_allocate();
    if(!file)
        return ENOMEM;

    int fd = fd_reserve();
    if(fd < 0) {
        fd_free(file);
        return -fd;
    }

    struct proc* proc = current_proc();
    assert(proc);

    mutex_acquire(&proc->fd_mutex);

    proc->fd_first = fd + 1;
    proc->fd[fd].file = file;
    proc->fd[fd].flags = flags;

    mutex_release(&proc->fd_mutex);

    *filep = file;
    *fdp = fd;

    return 0;
}

int fd_duplicate(int new_fd, int old_fd) {
    int err = 0;
    struct file *file = fd_get(old_fd);
    if(!file)
        return EBADF;
    
    if((err = fd_exact(new_fd))) {
        fd_release(file);
        return err;
    }

    struct proc* proc = current_proc();
    assert(proc);

    mutex_acquire(&proc->fd_mutex);

    proc->fd[new_fd].flags = proc->fd[old_fd].flags;
    if(!(proc->fd[new_fd].file = fd_allocate())) {
        err = ENOMEM;
        mutex_release(&proc->fd_mutex);

        fd_vacate(new_fd);
        goto cleanup;
    }

    memcpy(proc->fd[new_fd].file, file, sizeof(struct file));

    // reset reference count
    proc->fd[new_fd].file->ref_count = 1;

    mutex_release(&proc->fd_mutex);
cleanup:
    fd_release(file);
    return err;
}

int fd_close(int fd) {
    struct proc* proc = current_proc();
    assert(proc);

    mutex_acquire(&proc->fd_mutex);

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
    mutex_acquire(&proc->fd_mutex);

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

