#include <ringbuffer.h>

#include <errno.h>
#include <kernelio.h>
#include <math.h>
#include <mem/vmm.h>
#include <string.h>

#define RINGBUFFER_VMM_FLAGS VMM_FLAGS_ALLOCATE
#define RINGBUFFER_MMU_FLAGS (MMU_FLAGS_WRITE | MMU_FLAGS_READ | MMU_FLAGS_NOEXEC)

int ringbuffer_init(ringbuffer_t *ringbuffer, size_t size) {
    ringbuffer->data = vmm_map(nullptr, ROUND_UP(size, PAGE_SIZE), RINGBUFFER_VMM_FLAGS, RINGBUFFER_MMU_FLAGS, nullptr);
    if(!ringbuffer->data)
        return ENOMEM;

    memset(ringbuffer->data, 0, size);
    ringbuffer->size = size;
    ringbuffer->write = 0;
    ringbuffer->read = 0;
    return 0;
}

void ringbuffer_destroy(ringbuffer_t *ringbuffer) {
    vmm_unmap(ringbuffer->data, ROUND_UP(ringbuffer->size, PAGE_SIZE), 0);
}

size_t ringbuffer_read(ringbuffer_t *ringbuffer, void *buffer, size_t size) {
    size_t occupancy = ringbuffer_occupancy(ringbuffer);
    size = MIN(size, occupancy);
    
    size_t initial_offset = ringbuffer->read % ringbuffer->size;
    size_t inital_remaining = ringbuffer->size - initial_offset;
    size_t initial_size = MIN(size, inital_remaining);

    memcpy(buffer, ringbuffer->data + initial_offset, initial_size);

    if(initial_size != size)
        memcpy(buffer + initial_size, ringbuffer->data, size - initial_size);

    ringbuffer->read += size;
    return size;
}

size_t ringbuffer_write(ringbuffer_t* ringbuffer, void* buffer, size_t size) {
    size_t free_space = ringbuffer->size - ringbuffer_occupancy(ringbuffer);
    size = MIN(size, free_space);

    size_t initial_offset = ringbuffer->write % ringbuffer->size;
    size_t initial_remaining = ringbuffer->size - initial_offset;
    size_t initial_size = MIN(size, initial_remaining);

    memcpy(ringbuffer->data + initial_offset, buffer, initial_size);

    if(initial_size == size)
        memcpy(ringbuffer->data, buffer + initial_size, size - initial_size);

    ringbuffer->write += size;
    return size;
}

