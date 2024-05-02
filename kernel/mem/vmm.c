#include "kernelio.h"
#include "scheduling/thread.h"
#include <mem/vmm.h>
#include <mem/pmm.h>

void vmm_init(enum vmm_level level, struct vmm_info* info) {
    unimplemented();
}

void* vmm_alloc(size_t size, enum paging_flags flags, struct vmm_info* info) {
    unimplemented();
}

void vmm_free(void* ptr, size_t size, enum paging_flags flags) {
    unimplemented();
}
