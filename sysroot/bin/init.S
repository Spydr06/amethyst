.section .text
.globl _start
_start:
    mov $255, %rax
    mov $4, %rdi
    mov $hello_world, %rsi
    mov $13, %rdx
    syscall
.L.loop:
    jmp .L.loop

.section .rodata
.globl msg
hello_world:
    .ascii "Hello, World!"
    .byte 0

