.section .text
.globl _start
_start:
    //jmp _start // infinite loop
    mov $0, %rax
    syscall // test syscall

