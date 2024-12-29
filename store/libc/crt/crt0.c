__attribute__((section(".text"))) extern int main(int argc, char** argv, char** envp);

__attribute__((naked, used, section(".text")))
extern _Noreturn void _start(void) {
    __asm__ volatile (
        "lea main(%%rip), %%rdi;"
        "mov %%rsp, %%rsi;"
        "call __libc_entry;"
        :::"rdi", "rsi"
    );
}

