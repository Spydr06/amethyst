.section .text
.globl _start
_start:
    mov $255, %rax
    mov $4, %rdi
    mov $msg, %rsi
    mov $13, %rdx
    syscall
.L.loop:
    jmp .L.loop

.section .rodata
.globl msg
msg:
    .ascii "Hello, World!"
    .byte 0
msg_end:
.set msg_length, msg_end - msg
