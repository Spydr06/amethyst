static const char message[] = "Hello, World :3\n";

__attribute__((naked, section(".text"), used))
_Noreturn void _start(void)  {    
    __asm__ volatile (
            "mov %[num], %%rax;"
            "mov %[fd], %%rdi;"
            "mov %[buf], %%rsi;"
            "mov %[cnt], %%rdx;"
            "syscall;"
            ::  [num] "i" (1), [fd] "i" (1), [buf] "r" (message), [cnt] "r" (sizeof message)
            :   "rax","rdi","rsi","rdx"
    );

    __asm__ volatile (".l: jmp .l");
}

