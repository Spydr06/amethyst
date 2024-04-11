/*OUTPUT_FORMAT(elf64-x86-64)*/

KERNEL_BASE = 0xFFFFFFFF80000000;

ENTRY(_start)
SECTIONS
{
  PROVIDE(_ELF_START_ = . + 0x100000);
  PROVIDE(_LOAD_START_ = _ELF_START_);
  . = _ELF_START_;

  .init (_ELF_START_): AT(ADDR(.init)) {
      KEEP(*(.multiboot))
      *(.inittext)
  }

  PROVIDE(_KERNEL_BASE_ = KERNEL_BASE);
  . += KERNEL_BASE;
  
  .text ALIGN(0x1000) : AT(ADDR(.text) - KERNEL_BASE)
  {
    _TEXT_START_ = .;
    *(.text .text.*)
    _TEXT_END_ = .;
  }

  .rodata ALIGN(0x1000) : AT(ADDR(.rodata) - KERNEL_BASE)
  {
    _RODATA_START_ = .;
    *(.rodata .rodata.*)
    *(.gnu.linkonce.r*)
    _RODATA_END_ = .;
  }

  .init_array : AT(ADDR(.init_array) - KERNEL_BASE)
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
    KEEP (*(.init_array .ctors))
    PROVIDE_HIDDEN (__init_array_end = .);
  }
  /*.fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
    KEEP (*(.fini_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .dtors))
    PROVIDE_HIDDEN (__fini_array_end = .);
  }*/

  /* For stack unwinding (exception handling)  */
  .eh_frame_hdr ALIGN(0x8) : AT(ADDR(.eh_frame_hdr) - KERNEL_BASE)
  {
    KEEP(*(.eh_frame_hdr*))
  }

  .eh_frame ALIGN(0x8) : AT(ADDR(.eh_frame) - KERNEL_BASE)
  {
  	PROVIDE (__eh_frame_start = .);
    KEEP(*(.eh_frame))
    LONG (0);
  }
  .gcc_except_table : AT(ADDR(.gcc_except_table) - KERNEL_BASE)
  {
    *(.gcc_except_table)
  }

  .data ALIGN(0x1000) : AT(ADDR(.data) - KERNEL_BASE)
  {
    _DATA_START_ = .;
    *(.padata)
    *(.data .data.*)
    *(.gnu.linkonce.d*)
    _DATA_END_ = .;
  }

  PROVIDE(_LOAD_END_ = .);

  .bss : AT(ADDR(.bss) - KERNEL_BASE)
  {
    _BSS_START_ = .;
    *(.bss .bss.* .gnu.linkonce.b.*)
    *(COMMON)
    _BSS_END_ = .;
  }

  . = ALIGN(0x10);
  _end = .;
  
  /DISCARD/ : {
    *(.note .note.*)
    *(.comment)
  }
}
