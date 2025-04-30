#include <sys/loadavg.h>
#include <sys/scheduler.h>

static uintmax_t calc_load_tasks;

size_t avenrun[AVENRUN_COUNT];

void get_avenrun(size_t loads[AVENRUN_COUNT], size_t offset, int shift) {
    for(size_t i = 0; i < AVENRUN_COUNT; i++) {
        loads[i] = (avenrun[i] + offset) << shift;
    }
}

void calc_global_load(void) {
    size_t active = __atomic_load_n(&calc_load_tasks, __ATOMIC_SEQ_CST);
    size_t active_fix = active > 0 ? active * FIXED_1 : 0;

    avenrun[0] = calc_load(avenrun[0], EXP_1, active_fix);
    avenrun[1] = calc_load(avenrun[1], EXP_5, active_fix);
    avenrun[2] = calc_load(avenrun[2], EXP_15, active_fix);

    __atomic_store_n(&calc_load_tasks, 0, __ATOMIC_SEQ_CST);

#ifdef _LOADAVG_DEBUG
    klog(DEBUG, "active: %zu; load average: %zu.%03zu -- %zu.%03zu -- %zu.%03zu", active, 
            FIX_WHOLE(avenrun[0]), FIX_FRACT(avenrun[0]),
            FIX_WHOLE(avenrun[1]), FIX_FRACT(avenrun[1]),
            FIX_WHOLE(avenrun[2]), FIX_FRACT(avenrun[2])
        );
#endif
}

void calc_global_load_tick(void) {
    _cpu()->loadavg_ticks++;

    if(_cpu()->loadavg_ticks < LOAD_FREQ)
        return;
    
    _cpu()->loadavg_ticks -= LOAD_FREQ;

    size_t active = !(_cpu()->thread == _cpu()->idle_thread); 
    __atomic_add_fetch(&calc_load_tasks, active, __ATOMIC_SEQ_CST);

    // task one cpu to calculate the 'final' global load
    if(_cpu()->id == 0)
        calc_global_load();
}
