.section .text
.globl _gdt_reload
_gdt_reload:
    lgdt (%rdi)

    mov $0x28, %ax
    ltr %ax

    swapgs
    mov $0, %ax
    mov %ax, %gs
    mov %ax, %fs
    swapgs
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    pushq $0x8
    pushq $.L.reload
    retfq

.L.reload:
    ret

