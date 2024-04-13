#include <mem/pmm.h>
#include <mem/bitmap.h>
#include <sys/spinlock.h>

static spinlock_t memory_spinlock;

void pmm_setup(uintptr_t addr, uint32_t size) {
    bitmap_init(addr + size);
    uintptr_t bitmap_start_addr;
    size_t bitmap_size;
    bitmap_get_region(&bitmap_start_addr, &bitmap_size, ADDRESS_TYPE_PHYSICAL);
    spinlock_release(&memory_spinlock);
}

