.section .text
.globl _start
_start:
.L.loop:
    mov $1, %rax
    syscall

    jmp .L.loop

