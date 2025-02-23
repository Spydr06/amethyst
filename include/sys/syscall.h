#ifndef _AMETHYST_CPU_SYSCALLS_H
#define _AMETHYST_CPU_SYSCALLS_H

#include "x86_64/cpu/cpu.h"
#include <amethyst/stat.h>
#include <amethyst/syscall.h>
#include <cdefs.h>
#include <cpu/cpu.h>
#include <kernelio.h>
#include <sys/mmap.h>

#include <stdint.h>
#include <stddef.h>

#define __syscall __no_caller_saved_registers __general_regs_only

typedef struct {
    uint64_t ret;
    uint64_t _errno;
} syscallret_t;

typedef syscallret_t (*syscall_t)(...) __no_caller_saved_registers;

extern const size_t _syscall_count;

bool syscalls_init(void);

const char* _syscall_get_name(size_t i);
const char* _syscall_get_debug_fmt(size_t i);

extern void _syscall_entry(void);

__syscall syscallret_t _syscall_invalid(struct cpu_context* ctx);

// actual syscalls located in `kernel/sys/syscalls/`

__syscall syscallret_t _sys_read(struct cpu_context* ctx, int fd, void* buffer, size_t size);
__syscall syscallret_t _sys_write(struct cpu_context* ctx, int fd, const void* buffer, size_t size);
__syscall syscallret_t _sys_open(struct cpu_context* ctx, const char* path, int flags, mode_t mode);
__syscall syscallret_t _sys_close(struct cpu_context* ctx, int fd);
__syscall syscallret_t _sys_stat(struct cpu_context* ctx, const char* path, struct stat *statbuf);
__syscall syscallret_t _sys_fstat(struct cpu_context* ctx, int fd, struct stat *statbuf);
__syscall syscallret_t _sys_mkdir(struct cpu_context* ctx, const char* path, mode_t mode);
__syscall syscallret_t _sys_getcwd(struct cpu_context* ctx, char* cwd, size_t cwd_size);

__syscall syscallret_t _sys_mmap(struct cpu_context* ctx, void* addr, size_t len, enum map_prot prot, enum map_flags flags, int fd, off_t offset);
__syscall syscallret_t _sys_munmap(struct cpu_context* ctx, void* addr, size_t len);
__syscall syscallret_t _sys_brk(struct cpu_context* ctx, void* addr);

__syscall syscallret_t _sys_mount(struct cpu_context* ctx, char* u_backing, char* u_dir_name, char* u_type, register_t flags, void* data);
__syscall syscallret_t _sys_umount(struct cpu_context* ctx, const char* u_dir_name);

__syscall syscallret_t _sys_ioctl(struct cpu_context* ctx, int fd, unsigned long request, void* arg);

__syscall syscallret_t _sys_yield(struct cpu_context* ctx);

__syscall syscallret_t _sys_execve(struct cpu_context* ctx, const char *u_filename, const char *const argv[], const char *const u_envp[]);
__syscall syscallret_t _sys_exit(struct cpu_context* ctx, int exit_code);
__syscall syscallret_t _sys_knldebug(struct cpu_context* ctx, enum klog_severity severity, const char* user_buffer, size_t buffer_size);

#endif /* _AMETHYST_CPU_SYSCALLS_H */

