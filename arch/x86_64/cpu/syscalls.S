/*.extern _sys_read
.extern _sys_write
.extern _sys_open
.extern _sys_close
.extern _sys_stat
// ...

.section .rodata
.globl _syscall_table
.align 8
_syscall_table:
    .quad _sys_read
    .quad _sys_write
    .quad _sys_open
    .quad _sys_close
    .quad _sys_stat
    // ...
_syscall_table_end:

.globl _syscall_count
_syscall_count:
    .quad (_syscall_table_end - _syscall_table) / 8
*/
.section .text
.globl _syscall_entry
_syscall_entry:
    swapgs
    // TODO
    sysretq
