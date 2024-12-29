#include <stdint.h>

extern _Noreturn void __libc_entry(int (*main_fn)(int argc, char* argv[], char* env[]), uintptr_t* stack_base) {
    main_fn(0, 0, 0);

    int exit_code = 0;

    __asm__ volatile (
        "mov %0, %%rdi;"
        "mov $60, %%rax;"
        "syscall;"
        ::"m" (exit_code)
        : "rax", "rdi"
    );

    while(1);
}
