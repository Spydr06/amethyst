#ifndef _AMETHYST_SYS_PROC_H
#define _AMETHYST_SYS_PROC_H

#include <abi.h>

#include <sys/mutex.h>
#include <sys/spinlock.h>
#include <sys/semaphore.h>
#include <sys/fd.h>

#include <filesystem/virtual.h>

#define PROC_HOLD(v) (__atomic_add_fetch(&(v)->ref_count, 1, __ATOMIC_SEQ_CST))

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

    spinlock_t nodes_lock;

    semaphore_t wait_sem;
};

#endif /* _AMETHYST_SYS_PROC_H */

