#include <memory.h>

#include <mem/user.h>
#include <cpu/cpu.h>
#include <sys/thread.h>

struct memcpy_desc {
    void* dst;
    const void* src;
    size_t size;
};

static void memcpy_internal(struct cpu_context* ctx, void* userp) {
    struct memcpy_desc* desc = userp;

    memcpy(desc->dst, desc->src, desc->size);

    CPU_RET(ctx) = 0;
}

int memcpy_from_user(void *kernel_dest, const void *user_src, size_t size) {
    struct memcpy_desc desc = {
        .dst = kernel_dest,
        .src = user_src,
        .size = size
    };

    // TODO: check if `user_src` really is a user address

    return _context_save_and_call(memcpy_internal, nullptr, &desc);
}
