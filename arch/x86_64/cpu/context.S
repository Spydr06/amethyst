.section .text

.globl _context_switch
_context_switch:
    xor %rsi, %rsi
    call _sched_userspace_check
    cli

    mov %rdi, %rsp

    add $24, %rsp // ignore cr2, gs and fs
    pop %rax
    mov %rax, %es
    pop %rax
    mov %rax, %ds
    pop %rax
    pop %rbx
    pop %rcx
    pop %rdx
    pop %r8
    pop %r9
    pop %r10
    pop %r11
    pop %r12
    pop %r13
    pop %r14
    pop %r15
    pop %rdi
    pop %rsi
    pop %rbp
    add $8, %rsp

    cmpq $0x23, 8(%rsp)
    jne .L.not_needed
    swapgs
.L.not_needed:
    iretq

.globl _context_save_and_call
_context_save_and_call:
    mov (%rsp), %r11
    mov %rsp, %r10
    add $8, %r10

    test %rsi, %rsi
    js .L.no_stack_change
    mov %rsp, %rsi
.L.no_stack_change:
    
    pushq $0x10  // ss
    push %r10    // rsp
    pushf
    pushq $0x08  // cs
    push %r11    // return_addr
    sub $8, %rsp // error_code
    push %rbp
    push %rsi
    push %rdi
    push %r15
    push %r14
    push %r13
    push %r12
    push %r11
    push %r10
    push %r9
    push %r8
    push %rdx
    push %rcx
    push %rbx
    push %rax
    pushq $0x10  // es
    pushq $0x10  // ds
    pushq $0     // fs
    pushq $0     // gs
    sub $8, %rsp // cr2

    mov %rdi, %r11
    mov %rsp, %rdi
    mov %rdx, %rsi

    call *%r11

    cli
    mov %rsp, %rdi
    call _context_switch

