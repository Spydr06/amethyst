#include "kernelio.h"
#include <mem/pmm.h>
#include <sys/spinlock.h>
#include <mem/vmm.h>

static spinlock_t memory_spinlock;

void pmm_setup(uintptr_t addr, uint32_t size) {
    unimplemented();
}

void* pmm_alloc_frame(void) {
    unimplemented();
}