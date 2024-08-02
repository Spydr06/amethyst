.extern _sys_test
//.extern _sys_read
//.extern _sys_write
//.extern _sys_open
//.extern _sys_close
//.extern _sys_stat
// ...

.section .rodata

.globl _syscall_table
.align 8
_syscall_table:
    .quad _sys_test
  //  .quad _sys_read
  //  .quad _sys_write
  //  .quad _sys_open
  //  .quad _sys_close
  //  .quad _sys_stat
    // ...
_syscall_table_end:

.globl _syscall_count
_syscall_count:
    .quad (_syscall_table_end - _syscall_table) / 8

.section .text

.extern _sched_userspace_check

// syscall calling convention:
// %rax: syscall number
// ...

.globl _syscall_entry
_syscall_entry:
    // swapgs

    // save syscall number in cr2
    mov %rax, %cr2

    mov %gs:0, %rax // (struct cpu*) cpu
    movq (%rax), %rax  // (struct thread*) cpu->thread
    movq (%rax), %rax  // (void*) thread->kernel_stack_top

    xchg %rax, %rsp // switch stack to kernel

    // push context
    pushq $0x1b    // user %sp
    pushq %rax     // user %rsp
    mov %cr2, %rax // restore syscall number
    push %r11      // rflags
    pushq $0x23    // user %cs
    pushq %rcx     // save return %rip
    pushq $0       // error
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
    mov %ds, %rbx
    push %rbx
    mov %es, %rbx
    push %rbx
    mov %fs, %rbx
    push %rbx
    mov %gs, %rbx
    push %rbx
    mov %cr2, %rbx
    push %rbx

    mov $0x10, %rbx
    mov %rbx, %es
    mov %rbx, %ds

    push %r9
    mov %r8, %r9
    mov %r10, %r8
    mov %rdx, %rcx
    mov %rsi, %rdx
    mov %rdi, %rsi

    sti
    mov %rax, %rdi
    
    // TODO: log syscalls ?
    // call _syscall_log
    
    mov %rsp, %rdi
    add $8, %rdi

    cmp _syscall_count, %rax
    jae .L.no_syscall
    call *_syscall_table(,%rax,8)
    jmp .L.return

.L.no_syscall:
    call _syscall_invalid
.L.return:
    cli

    mov %rax, %rdi
    mov %rdx, %rsi

    // TODO: log syscalls?

    mov %rsp, %rdi
    add $8, %rdi
    xor %rsi, %rsi
    inc %rsi

    mov %rax, %rcx
    call _sched_userspace_check

    cli

    add $32, %rsp
    pop %rbx
    mov %rbx, %es
    pop %rbx
    mov %rbx, %ds
    add $8, %rsp // rax
    pop %rbx
    pop %rcx
    add $8, %rsp // rdx
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
    add $8, %rsp // remove error code
    pop %rcx
    add $8, %rsp // user %cs
    pop %r11     // rflags
    pop %rsp

    //swapgs

    sysretq
