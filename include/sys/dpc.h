#ifndef _AMETHYST_SYS_DPC_H
#define _AMETHYST_SYS_DPC_H

#include <cpu/cpu.h>

typedef void* dpc_arg_t;
typedef void (*dpc_fn_t)(struct cpu_context*, dpc_arg_t);

struct dpc {
    struct dpc *next, *prev;
    bool enqueued;
    dpc_fn_t callback;
    dpc_arg_t arg;
};

void dpc_init(void);

void dpc_enqueue(struct dpc* dpc, dpc_fn_t fn, dpc_arg_t arg);
void dpc_dequeue(struct dpc* dpc);

#endif /* _AMETHYST_SYS_DPC_H */

