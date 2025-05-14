.section .rodata

.globl geode_prelude_source
.align 16
geode_prelude_source:
    .incbin "geode/prelude.shard"
    .byte 0
__geode_prelude_source_end:

.globl geode_prelude_source_length
.align 16
geode_prelude_source_length:
.quad __geode_prelude_source_end - geode_prelude_source - 1

