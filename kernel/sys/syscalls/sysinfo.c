#include <sys/syscall.h>

#include <amethyst/sysinfo.h>
#include <mem/pmm.h>
#include <mem/user.h>
#include <sys/proc.h>
#include <sys/timekeeper.h>

#include <memory.h>
#include <errno.h>

__syscall syscallret_t _sys_sysinfo(struct cpu_context*, struct sysinfo* u_sysinfo) {
    syscallret_t ret = {
        .ret = -1,
        ._errno = 0
    };

    if(!u_sysinfo) {
        ret._errno = EFAULT;
    }

    struct sysinfo sysinfo;
    memset(&sysinfo, 0, sizeof(struct sysinfo));

    struct timespec ts = timekeeper_time_from_boot();
    sysinfo.uptime = ts.s;

    sysinfo.totalram = pmm_total_memory();
    sysinfo.freeram = pmm_free_memroy();

    sysinfo.procs = proc_count();
    // TODO:
    // sysinfo.loads
    // sysinfo.sharedram
    // sysinfo.bufferram
    // sysinfo.totalswap
    // sysinfo.freeswap
    // sysinfo.totalhigh
    // sysinfo.freehigh

    // FIXME: 1 Page???
    sysinfo.mem_unit = 1; // 1 Byte

    ret._errno = memcpy_to_user(u_sysinfo, &sysinfo, sizeof(struct sysinfo));
    ret.ret = 0;
    return ret;
}
