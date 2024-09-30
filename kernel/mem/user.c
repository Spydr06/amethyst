#include <memory.h>

#include <mem/user.h>
#include <cpu/cpu.h>
#include <sys/thread.h>

//
// memcpy
//

struct memcpy_desc {
    void* dst;
    const void* src;
    size_t size;
};

static void _memcpy(struct cpu_context* ctx, void* userp) {
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

    return _context_save_and_call(_memcpy, nullptr, &desc);
}

//
// strlen
//

struct strlen_desc {
    const char* str;
    size_t* size;
};

static void _strlen(struct cpu_context* ctx, void* userp) {
    struct strlen_desc* desc = userp;

    *desc->size = strlen(desc->str);
    
    CPU_RET(ctx) = 0;
}

int user_strlen(const char* user_str, size_t* size) {
    struct strlen_desc desc = {
        .str = user_str,
        .size = size
    };

    // TODO: check if `user_str` is really a user address
    
    return _context_save_and_call(_strlen, nullptr, &desc);
}

