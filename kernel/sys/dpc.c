#include <sys/dpc.h>
#include <cpu/interrupts.h>

#include <assert.h>

static void enqueue(struct dpc* dpc) {
    dpc->prev = nullptr;
    dpc->next = _cpu()->dpc_queue;

    if(dpc->next)
        dpc->next->prev = dpc;

    _cpu()->dpc_queue = dpc;
}

static void dequeue(struct dpc* dpc) {
    if(dpc->prev)
        dpc->prev->next = dpc->next;
    else
        _cpu()->dpc_queue = dpc->next;

    if(dpc->next)
        dpc->next->prev = dpc->prev;
}

static void isr_callback(struct cpu_context* context) {
    while(_cpu()->dpc_queue)
    {
        struct dpc* dpc = _cpu()->dpc_queue;
        dequeue(dpc);

        assert(dpc->enqueued);
        dpc->enqueued = false;

        interrupt_set(true);
        dpc->callback(context, dpc->arg);
        interrupt_set(false);
    }
}

void dpc_init(void) {
    _cpu()->dpc_isr = interrupt_allocate(isr_callback, nullptr, IPL_DPC);
    assert(_cpu()->dpc_isr);
}

void dpc_enqueue(struct dpc* dpc, dpc_fn_t callback, dpc_arg_t arg) {
    bool istate = interrupt_set(false);

    if(dpc->enqueued)
        goto cleanup;

    dpc->callback = callback;
    dpc->arg = arg;
    dpc->enqueued = true;

    enqueue(dpc);

    interrupt_raise(_cpu()->dpc_isr);

cleanup:
    interrupt_set(istate);
}

void dpc_dequeue(struct dpc* dpc) {
    bool istate = interrupt_set(false);

    if(!dpc->enqueued)
        goto cleanup;

    dpc->enqueued = false;
    dequeue(dpc);

cleanup:
    interrupt_set(istate);
}

