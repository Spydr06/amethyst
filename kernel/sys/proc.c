#include <sys/proc.h>

#include <mem/heap.h>
#include <hashtable.h>
#include <assert.h>

static hashtable_t pid_table;
static mutex_t pid_table_mutex;

static pid_t current_pid = 1;
static struct scache* proc_cache;

static inline struct vnode* lock_vnode(struct proc* proc, struct vnode** vnode_ptr) {
    bool int_status = interrupt_set(false);
    spinlock_acquire(&proc->nodes_lock);

    struct vnode* vnode = *vnode_ptr;
    vop_hold(vnode);

    spinlock_release(&proc->nodes_lock);
    interrupt_set(int_status);
    return vnode;
}

void proc_init(void) {
    proc_cache = slab_newcache(sizeof(struct proc), 0, nullptr, nullptr);
    assert(proc_cache);

    assert(hashtable_init(&pid_table, 100) == 0);
    mutex_init(&pid_table_mutex);
}

pid_t proc_new_pid(void) {
    return __atomic_fetch_add(&current_pid, 1, __ATOMIC_SEQ_CST);
}

struct proc* proc_create(void) {
    struct proc* proc = slab_alloc(proc_cache);
    if(!proc)
        return nullptr;

    memset(proc, 0, sizeof(struct proc));

    mutex_init(&proc->mutex);
    spinlock_init(proc->nodes_lock);
    
    proc->pid = __atomic_fetch_add(&current_pid, 1, __ATOMIC_SEQ_CST);

    proc->ref_count = 1;
    proc->state = PROC_STATE_NORMAL;
    proc->running_thread_count = 1;
    
    proc->fd_count = 3;
    proc->fd_first = 1;
    mutex_init(&proc->fd_mutex);

    proc->fd = kmalloc(sizeof(struct fd) * proc->fd_count);
    if(!proc->fd) {
        slab_free(proc_cache, proc);
        return nullptr;
    }

    proc->umask = 022;
    
    semaphore_init(&proc->wait_sem, 0);

    mutex_acquire(&pid_table_mutex, false);
    if(hashtable_set(&pid_table, proc, &proc->pid, sizeof(pid_t), true)) {
        mutex_release(&pid_table_mutex);
        kfree(proc->fd);
        slab_free(proc_cache, proc);
        return nullptr;
    }
    mutex_release(&pid_table_mutex);

    return proc;
}

struct vnode* proc_get_root(void) {
    struct proc* proc = _cpu()->thread->proc;
    return lock_vnode(proc, &proc->root);
}

struct vnode* proc_get_cwd(void) {
    struct proc* proc = _cpu()->thread->proc;
    return lock_vnode(proc, &proc->root);
}

