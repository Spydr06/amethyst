#include <mem/pmm.h>
#include <mem/bitmap.h>
#include <sys/spinlock.h>
#include <mem/vmm.h>

static spinlock_t memory_spinlock;

void pmm_setup(uintptr_t addr, uint32_t size) {
    bitmap_init(addr + size);
    uintptr_t bitmap_start_addr;
    size_t bitmap_size;
    bitmap_get_region(&bitmap_start_addr, &bitmap_size, ADDRESS_TYPE_PHYSICAL);
    spinlock_release(&memory_spinlock);
}

static bool frame_available(void) {
    return used_frames < bitmap_size;
}

void* pmm_alloc_frame(void) {
    if(!frame_available()) {
        return nullptr;
    }

    spinlock_acquire(&memory_spinlock);

    uint64_t frame = bitmap_request_frame();
    if(frame > 0) {
        bitmap_set_bit(frame);
        used_frames++;
        spinlock_release(&memory_spinlock);
        return (void*) (frame * PAGE_SIZE);
    }

    spinlock_release(&memory_spinlock);
    return nullptr;
}

void pmm_reserve_area(uintptr_t starting_address, size_t size) {
    uintptr_t location = starting_address / PAGE_SIZE;
    uint32_t number_of_frames = size / PAGE_SIZE;
    if(size % PAGE_SIZE != 0)
        number_of_frames++;

    spinlock_acquire(&memory_spinlock);

    for(; number_of_frames > 0; number_of_frames--) {
        if(!bitmap_test_bit(location)) {
            bitmap_set_bit(location++);
            used_frames++;
        }
    }

    spinlock_release(&memory_spinlock);
}
