#include <mem/vmm.h>

struct vmm_info vmm_kernel;

void* vmm_alloc(size_t size, enum paging_flags flags, struct vmm_info* info);
size_t align_value_to_page(size_t value);
bool is_address_aligned(size_t value, size_t align);

void* vmm_alloc_at(uintptr_t base_address, size_t size, enum paging_flags flags, struct vmm_info* info) {
    if(!info) {
        info = &vmm_kernel;
    }
}
