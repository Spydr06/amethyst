/*OUTPUT_FORMAT(elf64-x86-64)*/

KERNEL_BASE = 0xFFFFFFFF80000000;

ENTRY(_start)
SECTIONS
{ 
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

  /* Dynamic section for relocations, both in its own PHDR and inside data PHDR */
  .dynamic : {
    *(.dynamic)
  } :data :dynamic

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
