/* Default linker script for an amethyst kernel module */

SECTIONS
{
    .text : ALIGN(0x1000)
    {
        *(.text .text.*) 
    }

    .rodata : ALIGN(0x1000)
    {
        *(.rodata .rodata.*)
        *(.gnu.linkonce.r*)
    }

    .data : ALIGN(0x1000)
    {
        *(.data .data.*)
        *(.gnu.linkonce.d*)
    }

    .syscalls : ALIGN(0x8)
    {
        __syscalls_start__ = .;
        KEEP(*(.static_syscalls))
        __syscalls_end__ = .;
    }

    .modinfo : ALIGN(0x8)
    {
        *(.modinfo) 
    }

    .bss : ALIGN(0x1000)
    {
        *(.bss .bss.*)
        *(.gnu.linkonce.b*)
        *(COMMON)
    }

    /DISCARD/ :
    {
        *(.comment)
        *(.note.*)
        *(.eh_frame)
        *(.debug*)
    }
}

