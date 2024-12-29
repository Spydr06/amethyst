static const char message[] = "Hello, World <2\n";

int main() {
    __asm__ volatile (
        "mov %[num], %%rax;"
        "mov %[fd], %%rdi;"
        "mov %[buf], %%rsi;"
        "mov %[cnt], %%rdx;"
        "syscall;"
        ::  [num] "i" (1), [fd] "i" (1), [buf] "r" (message), [cnt] "r" ((sizeof message) - 1)
        :   "rax","rdi","rsi","rdx"
    );

    while(1);
    return 0;
}

