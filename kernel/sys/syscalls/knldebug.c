#include <sys/syscall.h>
#include <sys/thread.h>
#include <sys/proc.h>

#include <mem/user.h>
#include <mem/vmm.h>

#include <kernelio.h>

#include <string.h>
#include <errno.h>
#include <math.h>

extern __syscall syscallret_t _sys_knldebug(struct cpu_context* __unused, enum klog_severity severity, const char* user_buffer, size_t buffer_size) {
    if(!user_buffer)
        return (syscallret_t) {
            ._errno = EINVAL,
            .ret = -1
        };
    
    void* kernel_buffer = vmm_map(nullptr, MAX(buffer_size, 1), VMM_FLAGS_ALLOCATE, MMU_FLAGS_READ | MMU_FLAGS_WRITE | MMU_FLAGS_NOEXEC, nullptr);
    if(!kernel_buffer)
        return (syscallret_t) {
            ._errno = ENOMEM,
            .ret = -1
        };

    int err = memcpy_from_user(kernel_buffer, user_buffer, buffer_size);
    if(err)
        return (syscallret_t) {
            ._errno = err,
            .ret = -1
        };

    char proc_info[64] = "\e[1;4m[";
    utoa((uint64_t) current_thread()->tid, proc_info + strlen(proc_info), 10);
    strcat(proc_info, ":");
    utoa((uint64_t) current_proc()->pid, proc_info + strlen(proc_info), 10);
    strcat(proc_info, "]\e[22;24m");

    char fmt[32] = "%";
    utoa((uint64_t) buffer_size, fmt + 1, 10);
    strcat(fmt, "s");

    __klog(severity, proc_info, fmt, kernel_buffer); 
    return (syscallret_t){
        ._errno = 0,
        .ret = 0
    };
}

