.extern __interrupt_handler

.macro isr_err_stub num
isr_stub_\num:
    save_context
    mov %rsp, %rdi
    mov $\num, %rsi
    cld
    call __interrupt_handler
    restore_context
    iretq
.endm

.macro isr_no_err_stub num
isr_stub_\num:
    push $0
    save_context
    mov %rsp, %rdi
    mov $\num, %rsi
    cld
    call __interrupt_handler
    restore_context
    add $8, %rsp
    iretq
.endm

.macro save_context
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
    mov %ds, %rax
    push %rax
    mov %es, %rax
    push %rax
    mov %fs, %rax
    push %rax
    mov %gs, %rax
    push %rax
    mov %cr2, %rdi
    push %rdi
.endm

.macro restore_context
    add $24, %rsp // skip cr2, gs and fs
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
.endm

.section .text

isr_no_err_stub 0
isr_no_err_stub 1
isr_no_err_stub 2
isr_no_err_stub 3
isr_no_err_stub 4
isr_no_err_stub 5
isr_no_err_stub 6
isr_no_err_stub 7
isr_err_stub    8
isr_no_err_stub 9
isr_err_stub    10
isr_err_stub    11
isr_err_stub    12
isr_err_stub    13
isr_err_stub    14
isr_no_err_stub 15
isr_no_err_stub 16
isr_err_stub    17
isr_no_err_stub 18
isr_no_err_stub 19
isr_no_err_stub 20
isr_no_err_stub 21
isr_no_err_stub 22
isr_no_err_stub 23
isr_no_err_stub 24
isr_no_err_stub 25
isr_no_err_stub 26
isr_no_err_stub 27
isr_no_err_stub 28
isr_no_err_stub 29
isr_err_stub    30
isr_no_err_stub 31
isr_no_err_stub 32
isr_no_err_stub 33
isr_no_err_stub 34
isr_no_err_stub 128
isr_no_err_stub 255

.section .rodata
.globl isr_stub_table
isr_stub_table:
    .quad isr_stub_0
    .quad isr_stub_1
    .quad isr_stub_2
    .quad isr_stub_3
    .quad isr_stub_4
    .quad isr_stub_5
    .quad isr_stub_6
    .quad isr_stub_7
    .quad isr_stub_8
    .quad isr_stub_9
    .quad isr_stub_10
    .quad isr_stub_11
    .quad isr_stub_12
    .quad isr_stub_13
    .quad isr_stub_14
    .quad isr_stub_15
    .quad isr_stub_16
    .quad isr_stub_17
    .quad isr_stub_18
    .quad isr_stub_19
    .quad isr_stub_20
    .quad isr_stub_21
    .quad isr_stub_22
    .quad isr_stub_23
    .quad isr_stub_24
    .quad isr_stub_25
    .quad isr_stub_26
    .quad isr_stub_27
    .quad isr_stub_28
    .quad isr_stub_29
    .quad isr_stub_30
    .quad isr_stub_31
    .quad isr_stub_32
    .quad isr_stub_33
    .quad isr_stub_34
.org isr_stub_table + 128 * 8
    .quad isr_stub_128
.org isr_stub_table + 255 * 8
    .quad isr_stub_255

.section .text
.globl _idt_reload
_idt_reload:
    cli
    lidt (%rdi)
    sti
    ret

.globl _interrupt_disable
_interrupt_disable:
    cli
    ret

.globl _interrupt_enable
_interrupt_enable:
    sti
    ret

