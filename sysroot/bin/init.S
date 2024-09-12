.section .text
.globl _start
_start:
    mov $0, %rax
    syscall // test syscall

.L.loop:
    jmp .L.loop

