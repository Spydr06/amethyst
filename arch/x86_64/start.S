#include <multiboot2.h>
#include <video/vga.h>

#include "paging.h"

/* The kernel is linked to run at -2GB. This allows efficient addressing */
KERNEL_BASE = 0xFFFFFFFF80000000

MULTIBOOT_INFO_SIZE = 120

MULTIBOOT_STACK = 0x9D400

IA32_EFER       = 0xC0000080
IA32_FS_BASE    = 0xC0000082  

LOOP_LIMIT = 512

.extern _BSS_START_
.extern _BSS_END_
.extern tls

.extern kmain

.set MULTIBOOT_HEADER_SIZE, multiboot_end - multiboot
.set MULTIBOOT_CHECKSUM, -(MULTIBOOT2_HEADER_MAGIC + MULTIBOOT_ARCHITECTURE_I386 + MULTIBOOT_HEADER_SIZE)

.section .multiboot, "a"
.globl multiboot
multiboot:
	.align 8
    .long MULTIBOOT2_HEADER_MAGIC
	.long MULTIBOOT_ARCHITECTURE_I386
    .long MULTIBOOT_HEADER_SIZE 
	.long MULTIBOOT_CHECKSUM 
    
    .align 8
    .word MULTIBOOT_HEADER_TAG_FRAMEBUFFER
    .word 0
    .long 20
    .long INIT_VGA_WIDTH
    .long INIT_VGA_HEIGHT
    .long INIT_VGA_DEPTH

    .align 8
    .word 0
    .word 0
    .long 8
multiboot_end:

.section .inittext, "ax"
.globl _start
.code32
_start:
	// save multiboot state
	mov %eax, multiboot_sig - KERNEL_BASE
	mov %ebx, multiboot_ptr - KERNEL_BASE

    // enable 32-bit stack
    mov $(stack_top - KERNEL_BASE), %esp
    mov %esp, %ebp

    // enable cpu features
    call enable_cpu_feat    

    // zero .bss
    mov $(_BSS_START_ - KERNEL_BASE), %edi
    mov $(_BSS_END_ - KERNEL_BASE), %ecx
    sub $(_BSS_START_ - KERNEL_BASE), %ecx
    mov $0, %eax
    rep stosb

    //enable stack protector
    rdtsc
    mov %eax, 0x1014

    jmp begin_enter_long_mode

enable_cpu_feat:
	// ensure that the CPU support long mode
	mov $0x80000000, %eax
	cpuid
	// check if CPUID supports the field we want to query
	cmp $0x80000001, %eax
	jbe .L.not_64bit_capable
	// test the IA-32e bit
	mov $0x80000001, %eax
	cpuid
	test $0x20000000, %edx /* bit 29 = */
	jz .L.not_64bit_capable

    // enable SSE
    // TODO: check if supported
    mov %cr0, %eax
    and $0xfffb, %ax // clear coprocessor emulation (cr0.em)
    or $2, %ax       // set coprocessor monitoring (cr0.mp)
    mov %eax, %cr0
    mov %cr4, %eax
    or $(3 << 9), %ax // set cr4.OSFXSR and cr4.OSXMMEXCPT
    or $0x20, %ax     // enable native FPU exception handling
    mov %eax, %cr4

    // read out CPU features
    mov $1, %eax
    xor %ecx, %ecx
    cpuid
    mov %ecx, %edx
    
    // check for XSAVE support (bit 26)
    and $0x04000000, %ecx
    jz .L.xsave_not_supported
    // enable XSAVE
    mov %cr4, %eax
    or $0x40000, %eax
    mov %eax, %cr4

.L.xsave_not_supported:
    // check for AVX support (bit 28)
    and $0x10000000, %edx
    jz .L.avx_not_supported
    // enable AVX
    xor %ecx, %ecx
    xgetbv
    or $7, %eax
    xsetbv
    
.L.avx_not_supported:
    ret

.L.not_64bit_capable:
	mov $0x3f8, %dx
	mov $'N', %al ; outb %al, %dx
	movw $0x100|'N', 0xb8000
	mov $'o', %al ; outb %al, %dx
	movw $0x100|'o', 0xb8002
	mov $'t', %al ; outb %al, %dx
	movw $0x100|'t', 0xb8004
	mov $'6', %al ; outb %al, %dx
	movw $0x100|'6', 0xb8006
	mov $'4', %al ; outb %al, %dx
	movw $0x100|'4', 0xb8008
	
.L.not_64bit_capable.loop:
	hlt
	jmp .L.not_64bit_capable.loop

begin_enter_long_mode:
	// set up state for long mode
    mov $(p3_table - KERNEL_BASE), %eax
    or $(PRESENT_BIT | WRITE_BIT), %eax
    mov %eax, (p4_table - KERNEL_BASE)

    mov $(p3_table_hh - KERNEL_BASE), %eax
    or $(PRESENT_BIT | WRITE_BIT), %eax
    mov %eax, (p4_table - KERNEL_BASE + 511 * 8)

    mov $(p4_table - KERNEL_BASE), %eax
    or $(PRESENT_BIT | WRITE_BIT), %eax
    mov %eax, (p4_table - KERNEL_BASE + 510 * 8)

    mov $(p2_table - KERNEL_BASE), %eax
    or $(PRESENT_BIT | WRITE_BIT), %eax
    mov %eax, (p3_table - KERNEL_BASE)

    mov $(p2_table - KERNEL_BASE), %eax
    or $(PRESENT_BIT | WRITE_BIT), %eax
    mov %eax, (p3_table_hh - KERNEL_BASE + 510 * 8)

    // counter
    mov $0, %ecx

.L.map_p2_table:
    mov $PAGE_SIZE, %eax
    mul %ecx
    or $PAGE_TABLE_ENTRY, %eax

    movl %eax, p2_table - KERNEL_BASE (,%ecx,8)

    inc %ecx
    cmp $LOOP_LIMIT, %ecx
    jne .L.map_p2_table

    mov $(p4_table - KERNEL_BASE), %eax
    mov %eax, %cr3

    // enable PAE (physical address extension)
    mov %cr4, %eax
    or $(1 << 5), %eax // PAE bit
    mov %eax, %cr4

	// enable IA-32e mode (Also enables SYSCALL and NX)
	mov $IA32_EFER, %ecx
	rdmsr // read model-specific register
	or $(1 << 8), %eax     /* LME */
	wrmsr // write model-specific register

    mov %cr0, %eax
    or $(1 << 31), %eax // paging bit
    or $(1 << 16), %eax // write protect
    mov %eax, %cr0

	lgdt gdtr_low - KERNEL_BASE
	ljmp $0x08, $long_mode

.code64

long_mode:
	// set up segment registers
	mov $0x10, %ax
	mov %ax, %ss
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs

    // Running in 64-bit mode, jump to high memory
    lgdt gdtr_high
	mov $long_mode_high, %rax
	jmp *%rax

.section .text
long_mode_high:
	// clear low-memory mapping
	mov $0, %eax
    movl %eax, p4_table - KERNEL_BASE 

	// set up segment registers
	mov $0x10, %ax
	mov %ax, %ss
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs
	
	// set up stack pointer
    mov $stack_top, %rsp
    push $0
    push $0
    mov %rsp, %rbp

    // enable stack protector
    mov $IA32_FS_BASE, %ecx
    mov $0, %edx
    mov tls, %eax
    wrmsr
    // set random stack protector value
    rdtsc
    mov $__SSP__, %rcx
    xor %rcx, %rax
    mov $tls, %rbx
    movq %rax, 0x28(%rbx)

    // call kernel entry
	call kmain

    cli
.L.long_mode.loop:
	hlt
	jmp .L.long_mode.loop

.section .bss
.align 4096
.globl p4_table
p4_table:
    .space 4096
.globl p3_table
p3_table:
    .space 4096
.globl p3_table_hh
p3_table_hh:
    .space 4096
.globl p2_table
p2_table:
    .space 4096
.align 4096
.globl end_of_mapped_memory
end_of_mapped_memory:
    .quad 0
stack:
    .space 16384
stack_top:

.section .data
.globl multiboot_sig
multiboot_sig:	.long 0
.globl multiboot_ptr
multiboot_ptr:  .long 0

GDT_WRITABLE     = (1 << 41)
GDT_EXECUTABLE   = (1 << 43)
GDT_CODE_OR_DATA = (1 << 44)
GDT_USERMODE     = (1 << 45)
GDT_PRESENT      = (1 << 47)
GDT_64BIT        = (1 << 53)

// Global Descriptor Table
gdtr_low:
	.word gdt_end - gdt - 1
	.long gdt - KERNEL_BASE
gdtr_high:
	.word gdt_end - gdt - 1
	.quad gdt
.globl gdt
gdt:
	.quad 0 // boot entry
.globl gdt_code
gdt_code:
    .quad GDT_CODE_OR_DATA | GDT_PRESENT | GDT_WRITABLE | GDT_EXECUTABLE | GDT_64BIT
.globl gdt_data
gdt_data:
    .quad GDT_CODE_OR_DATA | GDT_PRESENT | GDT_WRITABLE 
.globl gdt_user_code
gdt_user_code:
    .quad GDT_CODE_OR_DATA | GDT_PRESENT | GDT_WRITABLE | GDT_EXECUTABLE | GDT_64BIT | GDT_USERMODE
.globl gdt_user_data
gdt_user_data:
    .quad GDT_CODE_OR_DATA | GDT_PRESENT | GDT_WRITABLE | GDT_USERMODE
.globl gdt_tss_low
gdt_tss_low:
    .quad 0
.globl gdt_tss_high
gdt_tss_high:
    .quad 0
.globl gdt_end
gdt_end:

