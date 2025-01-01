#include <ringbuffer.h>

#include <mem/vmm.h>
#include <errno.h>
#include <math.h>

#define RINGBUFFER_VMM_FLAGS VMM_FLAGS_ALLOCATE
#define RINGBUFFER_MMU_FLAGS (MMU_FLAGS_WRITE | MMU_FLAGS_READ | MMU_FLAGS_NOEXEC)

int ringbuffer_init(ringbuffer_t *ringbuffer, size_t size) {
    ringbuffer->data = vmm_map(nullptr, ROUND_UP(size, PAGE_SIZE), RINGBUFFER_VMM_FLAGS, RINGBUFFER_MMU_FLAGS, nullptr);
    if(!ringbuffer->data)
        return ENOMEM;

    ringbuffer->size = size;
    ringbuffer->write = 0;
    ringbuffer->read = 0;
    return 0;
}

void ringbuffer_destroy(ringbuffer_t *ringbuffer) {
    vmm_unmap(ringbuffer->data, ROUND_UP(ringbuffer->size, PAGE_SIZE), 0);
}

size_t ringbuffer_read(ringbuffer_t *ringbuffer, void *buffer, size_t count) {
    size_t occupancy = ringbuffer_occupancy(ringbuffer);
    count = MIN(count, occupancy);

    size_t first_pass_offset = ringbuffer->read % ringbuffer->size;
    size_t first_pass_remaining = ringbuffer->size - first_pass_offset;
    size_t first_pass_count = MIN(count, first_pass_remaining);


}