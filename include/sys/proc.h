#ifndef _AMETHYST_SYS_PROC_H
#define _AMETHYST_SYS_PROC_H

#include <abi.h>

#include <sys/mutex.h>
#include <sys/spinlock.h>
#include <sys/semaphore.h>
#include <sys/fd.h>
#include <sys/thread.h>
#include <cpu/cpu.h>

#include <filesystem/virtual.h>

#define PROC_HOLD(v) do {                                                   \
        int rc = __atomic_add_fetch(&(v)->ref_count, 1, __ATOMIC_SEQ_CST);  \
        /* klog(WARN, "[pid %d] +rc %d", (v)->pid, rc); */                  \
        (void) rc;                                                          \
    } while(0)

#define PROC_RELEASE(v) do {                                                \
        int rc = __atomic_sub_fetch(&(v)->ref_count, 1, __ATOMIC_SEQ_CST);  \
        /* klog(WARN, "[pid %d] -rc %d", (v)->pid, rc); */                  \
        if(rc == 0) {                                                       \
            proc_delete((v));                                               \
            (v) = nullptr;                                                  \
        }                                                                   \
    } while(0)

enum proc_state {
    PROC_STATE_NORMAL,
    PROC_STATE_ZOMBIE
};

struct proc {
    mutex_t mutex;
    int ref_count;
    int status;
    enum proc_state state;

    pid_t pid;
    struct cred cred;

    size_t running_thread_count;

    size_t fd_count;
    uintmax_t fd_first;
    mutex_t fd_mutex;
    struct fd* fd;
    mode_t umask;

    struct vnode* cwd;
    struct vnode* root;
    spinlock_t nodes_lock;

    semaphore_t wait_sem;
    spinlock_t exiting;

    struct proc *parent, *sibling, *child;
};

static inline struct proc* current_proc(void) {
    struct thread* thread = current_thread();
    return thread ? thread->proc : nullptr;
}

static inline pid_t current_pid(void) {
    struct proc* p = current_proc();
    return p ? p->pid : -1;
}

static inline mode_t umask(mode_t mode) {
    return mode & ~current_proc()->umask;
}

void proc_init(void);

pid_t proc_new_pid(void);

struct proc* proc_create(void);
void proc_delete(struct proc*);

struct vnode* proc_get_root(void);
struct vnode* proc_get_cwd(void);

size_t proc_count(void);

#endif /* _AMETHYST_SYS_PROC_H */

