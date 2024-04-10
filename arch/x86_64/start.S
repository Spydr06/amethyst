#include <multiboot.h>

/* The kernel is linked to run at -2GB. This allows efficient addressing */
KERNEL_BASE = 0xFFFFFFFF80000000

MULTIBOOT_INFO_SIZE = 120

MULTIBOOT_STACK = 0x9D400

IA32_EFER       = 0xC0000080
IA32_FS_BASE    = 0xC0000082  

.extern _BSS_START_
.extern _BSS_END_
.extern tls

.extern kmain

.section .multiboot, "a"
.globl multiboot
multiboot:
	.long MULTIBOOT_HEADER_MAGIC
	.long MULTIBOOT_HEADER_FLAGS
	.long MULTIBOOT_CHECKSUM
	.long multiboot
	.long 0, 0, 0, 0	/* load_addr, load_end_addr, bss_end_addr, entry_addr */
	/* Video mode */
	.long 0 	/* Mode type (0: LFB) */
	.long 0 	/* Width (no preference) */
	.long 0 	/* Height (no preference) */
	.long 32	/* Depth (32-bit preferred) */

/* === Code === */
.section .inittext, "ax"
.globl _start
.code32
_start:
	// save multiboot state
	mov %eax, multiboot_sig - KERNEL_BASE
	mov %ebx, multiboot_ptr - KERNEL_BASE

    // enable 32-bit stack
    mov $MULTIBOOT_STACK, %esp
    mov %esp, %ebp

    // enable cpu features
    call enable_cpu_feat    

    // zero .bss
    mov _BSS_START_ - KERNEL_BASE, %edi
    mov _BSS_END_ - KERNEL_BASE, %ecx
    sub _BSS_START_ - KERNEL_BASE, %ecx
    mov $0, %eax
    rep stosb

    // enable stack protector
    rdtsc
    mov %eax, 0x1014

    call begin_enter_long_mode

    cli
    hlt

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
	/* Enable:
	    PGE (Page Global Enable)
	  + PAE (Physical Address Extension)
	  + PSE (Page Size Extensions)
	*/
	mov %cr4, %eax
	or $(0x80|0x20|0x10), %eax
	mov %eax, %cr4

	// load PDP4
	mov $(init_pml4 - KERNEL_BASE), %eax
	mov %eax, %cr3

	// enable IA-32e mode (Also enables SYSCALL and NX)
	mov $IA32_EFER, %ecx
	rdmsr
	or $(1 << 11)|(1 << 8)|(1 << 0), %eax     /* NXE, LME, SCE */
	wrmsr

	// enable paging and enter long mode
	mov %cr0, %eax
	or $0x80010000, %eax      /* PG & WP */
	mov %eax, %cr0

	lgdt gdtr_low - KERNEL_BASE
	ljmp $0x08, $long_mode

.code64

long_mode:
	/* Running in 64-bit mode, jump to high memory */
	lgdt gdtr_high
	mov $long_mode_high, %rax
	jmp *%rax

.section .text
long_mode_high:
	// clear low-memory mapping
	mov $0, %rax
	mov %rax, init_pml4 - KERNEL_BASE + 0
	
	// set up segment registers
	mov $0x10, %ax
	mov %ax, %ss
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs
	
	// set up stack pointer
    mov $init_stack, %rsp
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

/* === Page-aligned data === */
.section .padata
/* Initial paging structures, four levels */
/* The +3 for sub-pages indicates "present (1) + writable (2)" */
init_pml4:
	.quad low_pdpt - KERNEL_BASE + 3	/* low map for startup, will be cleared before rust code runs */
	.rept 512 - 3
		.quad 0
	.endr
	.quad 0 	/* If you so wish, this is a good place for the "Fractal" mapping */
	.quad init_pdpt - KERNEL_BASE + 3	/* Final mapping */
low_pdpt:
	.quad init_pd - KERNEL_BASE + 3	/* early init identity map */
	.rept 512 - 1
		.quad 0
	.endr
init_pdpt:	/* covers the top 512GB, 1GB each entry */
	.rept 512 - 2
		.quad 0
	.endr
	.quad init_pd - KERNEL_BASE + 3	/* at -2GB, identity map the kernel image */
	.quad 0
init_pd:
	/* 0x80 = Page size extension */
	.quad 0x000000 + 0x80 + 3	/* Map 2MB, enough for a 1MB kernel */
	.quad 0x200000 + 0x80 + 3	/* - give it another 2MB, just in case */
	.rept 512 - 2
		.quad 0
	.endr 
init_stack_base:
	.rept 0x1000 * 2
		.byte 0
	.endr
init_stack:

.section .data
.globl multiboot_sig
multiboot_sig:	.long 0
.globl multiboot_ptr
multiboot_ptr:  .long 0

// Global Descriptor Table
gdtr_low:
	.word gdt_end - gdt - 1
	.long gdt - KERNEL_BASE
gdtr_high:
	.word gdt_end - gdt - 1
	.quad gdt
.globl gdt
gdt:
	.long 0, 0
        .long 0x00000000, 0x00209A00	/* 0x08: 64-bit Code */
        .long 0x00000000, 0x00009200    /* 0x10: 64-bit Data */
        .long 0x00000000, 0x0040FA00    /* 0x18: 32-bit User Code */
        .long 0x00000000, 0x0040F200    /* 0x20: User Data        */
        .long 0x00000000, 0x0020FA00    /* 0x28: 64-bit User Code       */
        .long 0x00000000, 0x0000F200    /* 0x30: User Data (64 version) */
gdt_end:

