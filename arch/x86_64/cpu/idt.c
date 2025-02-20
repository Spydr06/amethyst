#include "sys/scheduler.h"
#include <x86_64/cpu/idt.h>
#include <x86_64/dev/pic.h>
#include <x86_64/cpu/cpu.h>
#include <sys/syscall.h>
#include <drivers/char/keyboard.h>

#include <cpu/cpu.h>
#include <mem/vmm.h>
#include <cdefs.h>
#include <assert.h>
#include <stdint.h>

#include <kernelio.h>

extern void _idt_reload(volatile const struct idtr* idtr);
extern void _interrupt_enable(void);
extern void _interrupt_disable(void);

#define SAVE_RUN_PENDING() _context_save_and_call(run_pending, nullptr, nullptr)

static void run_pending(struct cpu_context* ctx, void* __unused);
static inline void isr_enqueue(struct isr* isr);

extern uint64_t _millis;

__aligned(0x10) struct interrupt_descriptor idt[256];

const struct idtr idtr = {
    .size = sizeof(idt) - 1,
    .idt = (uintptr_t) (void*) idt
};

static const char *exception_names[] = {
  "Divide by Zero Error",
  "Debug",
  "Non Maskable Interrupt",
  "Breakpoint",
  "Overflow",
  "Bound Range",
  "Invalid Opcode",
  "Device Not Available",
  "Double Fault",
  "Coprocessor Segment Overrun",
  "Invalid TSS",
  "Segment Not Present",
  "Stack-Segment Fault",
  "General Protection Fault",
  "Page Fault",
  "Reserved",
  "x87 Floating-Point Exception",
  "Alignment Check",
  "Machine Check",
  "SIMD Floating-Point Exception",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Security Exception",
  "Reserved"
};

static void load_default(void);

static void tick(struct cpu_context* status __unused) {
    _millis++;
}

void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags) {
    idt[vector].offset_1 = (uintptr_t) isr & 0xffff;
    idt[vector].offset_2 = ((uintptr_t) isr >> 16) & 0xffff;
#ifdef __x86_64__
    idt[vector].offset_3 = ((uintptr_t) isr >> 32) & 0xffffffff;
#endif
    idt[vector].selector = IDT_SEGMENT_SELECTOR;
    idt[vector].type_attributes = flags;
}

void init_interrupts(void) {
    extern void* isr_stub_table[];
    for(size_t i = 0; i < 256; i++) {
        idt_set_descriptor(i, isr_stub_table[i], IDT_PRESENT_FLAG | IDT_INTERRUPT_TYPE_FLAG);
    }
    
    interrupts_apinit();
    
    interrupt_register(PIT_INTERRUPT, tick, pic_send_eoi, IPL_TIMER);
}

void interrupts_apinit(void) {
    _cpu()->ipl = IPL_NORMAL;
    
    _idt_reload(&idtr);
    interrupt_set(true);

    load_default();
}

void idt_reload(void) {
    _idt_reload(&idtr);
}

static void double_fault_handler(struct cpu_context* status) {
    klog(ERROR, "Double Fault at %p.", (void*) status->error_code);
}

static void divide_error_handler(struct cpu_context* status) {
    panic_r(status, "Division by Zero (%lu).", status->error_code);
}

static void load_default(void) {
    interrupt_register(DOUBLE_FAULT, double_fault_handler, nullptr, IPL_NORMAL);
    interrupt_register(DIVIDE_ERROR, divide_error_handler, nullptr, IPL_NORMAL);
}

void interrupt_register(uint8_t vector, void (*handler)(struct cpu_context*), void (*eoi_handler)(uint32_t), enum ipl priority) {
    _cpu()->isr[vector] = (struct isr) {
        .id = (uint64_t) _cpu()->id << 32 | vector,
        .handler = handler,
        .eoi_handler = eoi_handler,
        .priority = priority,
        .pending = false,
        .next = nullptr,
        .prev = nullptr
    };
}

struct isr* interrupt_allocate(void (*handler)(struct cpu_context *), void (*eoi_handler)(uint32_t), enum ipl priority) {
    struct isr* isr = nullptr;

    for(size_t i = 32; i < 0x100; i++) {
        if(!_cpu()->isr[i].handler) {
            isr = &_cpu()->isr[i];
            interrupt_register(i, handler, eoi_handler, priority);
            break;
        }
    }

    return isr;
}

void interrupt_raise(struct isr* isr) {
    bool istate = interrupt_set(false);

    if(isr->pending)
        goto cleanup;

    isr_enqueue(isr);
    isr->pending = true;

cleanup:
    interrupt_set(istate);
}

enum ipl interrupt_raise_ipl(enum ipl ipl) {
    bool old_int_status = interrupt_set(false);

    enum ipl old_ipl = _cpu()->ipl;
    if(old_ipl > ipl)
        _cpu()->ipl = ipl;

    interrupt_set(old_int_status);
    return old_ipl;
}

enum ipl interrupt_lower_ipl(enum ipl ipl) {
    bool old_int_status = interrupt_set(false);

    enum ipl old_ipl = _cpu()->ipl;
    if(old_ipl < ipl)
        _cpu()->ipl = ipl;

    interrupt_set(old_int_status);

    if(old_int_status)
        SAVE_RUN_PENDING();

    return old_ipl;
}

static inline void isr_run(struct isr* isr, struct cpu_context* ctx) {
    enum ipl old_ipl = IPL_IGNORE;
    if(isr->priority != IPL_IGNORE)
        old_ipl = interrupt_raise_ipl(isr->priority);

    isr->handler(ctx);
    interrupt_set(false);

    if(isr->priority != IPL_IGNORE)
        interrupt_lower_ipl(old_ipl);
}

static inline void isr_enqueue(struct isr* isr) {
    if(isr->pending)
        return;

    isr->next = _cpu()->isr_queue;
    if(isr->next)
        isr->next->prev = isr;
    _cpu()->isr_queue = isr;
}

static inline void isr_dequeue(struct isr* isr) {
    if(isr->prev)
        isr->prev->next = isr->next;
    else
        _cpu()->isr_queue = isr->next;

    if(isr->next)
        isr->next->prev = isr->prev;
}

static void run_pending(struct cpu_context* ctx, void* __unused) {
    bool entry_status = _cpu()->interrupt_status;
    if(entry_status) {
        _interrupt_disable();
        _cpu()->interrupt_status = false;
    }

    struct isr* list = nullptr;
    struct isr* iter = _cpu()->isr_queue;

    while(iter) {
        struct isr* next = iter->next;
        if(_cpu()->ipl > iter->priority) {
            isr_dequeue(iter);

            if(list)
                list->prev = iter;
            iter->next = list;
            iter->prev = nullptr;
            list = iter;

            assert(iter->pending);
        }

        iter = next;
    }

    if(!list)
        goto finish;

    while(list) {
        struct isr* isr = list;
        list = list->next;
        if(list)
            list->prev = nullptr;

        isr->pending = false;
        isr_run(isr, ctx);

        // TODO: _could_ lead to infinite recursion
        run_pending(ctx, nullptr);
    }

finish:
    if(entry_status) {
        _cpu()->interrupt_status = true;
        _interrupt_enable();
    }
}

extern void __isr(struct cpu_context* ctx, register_t vector) {
    struct isr* isr = &_cpu()->isr[vector];
    _cpu()->interrupt_status = false;

    if(!isr->handler) {
        if(vector == 32)
            return; // mystery interrupt lol
        if(vector < __len(exception_names))
            panic_r(ctx, "Unhandled Interrupt %llu: %s.", (unsigned long long) vector, exception_names[vector]);
        else
            panic_r(ctx, "Unhandled Interrupt %llu.", (unsigned long long) vector);
    }

    if(_cpu()->ipl > isr->priority) {
        isr_run(isr, ctx);
        run_pending(ctx, nullptr);
    }
    else {
        isr_enqueue(isr);
        isr->pending = true;
    }

    if(isr->eoi_handler)
        isr->eoi_handler(vector);

    if(cpu_ctx_is_user(ctx))
        _sched_userspace_check(ctx, false, 0, 0);

    _cpu()->interrupt_status = CPU_CONTEXT_INTSTATUS(ctx);
}

bool interrupt_set(bool status) {
    bool old = _cpu()->interrupt_status;
    _cpu()->interrupt_status = status;

    if(status) {
        _interrupt_enable();
        SAVE_RUN_PENDING();
    }
    else
        _interrupt_disable();

    return old;
}

void idt_change_eoi(void (*eoi_handler)(uint32_t isr)) {
    bool before = interrupt_set(false);
       
    struct isr* handlers = _cpu()->isr;

    for(size_t i = 0; i < 0x100; i++) {
        if(handlers[i].eoi_handler)
            handlers[i].eoi_handler = eoi_handler;
    }

    interrupt_set(before);
}
