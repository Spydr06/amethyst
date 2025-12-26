/* Default linker script for an amethyst kernel module */

PHDRS
{
    text    PT_LOAD    FLAGS((1 << 0) | (1 << 2)) ; /* Execute + Read */
    rodata  PT_LOAD    FLAGS((1 << 2)) ;            /* Read only */
    data    PT_LOAD    FLAGS((1 << 1) | (1 << 2)) ; /* Write + Read */
}

SECTIONS
{
    .text : ALIGN(0x1000)
    {
        *(.text .text.*) 
    } :text

    .rodata : ALIGN(0x1000)
    {
        *(.rodata .rodata.*)
        *(.gnu.linkonce.r*)
    } :rodata

    .data : ALIGN(0x1000)
    {
        *(.data .data.*)
        *(.gnu.linkonce.d*)
    } :data

    .syscalls : ALIGN(0x8)
    {
        __syscalls_start__ = .;
        KEEP(*(.static_syscalls))
        __syscalls_end__ = .;
    } :data

    .modinfo : ALIGN(0x8)
    {
        *(.modinfo) 
    } :data

    .bss : ALIGN(0x1000)
    {
        *(.bss .bss.*)
        *(.gnu.linkonce.b*)
        *(COMMON)
    } :data

    /DISCARD/ :
    {
        *(.comment)
        *(.note.*)
        *(.eh_frame)
        *(.debug*)
    }
}

