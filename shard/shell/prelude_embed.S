.section .rodata

.globl shell_prelude_source
.align 16
shell_prelude_source:
    .incbin "shell/prelude.shard"
    .byte 0
__shell_prelude_source_end:

.globl shell_prelude_source_length
.align 16
shell_prelude_source_length:
.quad __shell_prelude_source_end - shell_prelude_source - 1

