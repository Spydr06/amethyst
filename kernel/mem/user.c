#include <memory.h>

#include <mem/user.h>
#include <cpu/cpu.h>
#include <sys/thread.h>
#include <errno.h>

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

    if(!is_userspace_addr(user_src))
        return EFAULT;

    return _context_save_and_call(_memcpy, nullptr, &desc);
}

int memcpy_to_user(void* user_dest, const void* kernel_src, size_t size) {
    struct memcpy_desc desc = {
        .dst = user_dest,
        .src = kernel_src,
        .size = size
    };

    if(!is_userspace_addr(user_dest))
        return EFAULT;

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

    if(!is_userspace_addr(user_str))
        return EFAULT;
    
    return _context_save_and_call(_strlen, nullptr, &desc);
}

