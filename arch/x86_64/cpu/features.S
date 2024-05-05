.section .text
.globl _enable_sse
_enable_sse:
    mov %cr0, %rax
    and $0xfffb, %ax
    or $2, %eax
    mov %rax, %cr0
    mov %cr4, %rax
    or $(3 << 9), %rax
    mov %rax, %cr4
    ret

.globl _enable_x87_fpu
_enable_x87_fpu:
    fninit
    mov %cr0, %rax
    or $(1 << 5), %rax
    mov %rax, %cr0
    ret

.globl _enable_fxsave
_enable_fxsave:
    fxsave _fxsave_region
    ret

.section .data
.align 16
_fxsave_region:
    .zero 512

