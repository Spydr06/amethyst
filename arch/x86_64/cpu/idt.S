.extern __isr

.macro isr_err_stub num
isr_stub_\num:
    push %rbp
    mov $\num, %rbp
    jmp __isr_common
.endm

.macro isr_no_err_stub num
isr_stub_\num:
    pushq $0
    push %rbp
    mov $\num, %rbp
    jmp __isr_common
.endm

.macro save_context
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
    mov %ds, %rax
    push %rax
    mov %es, %rax
    push %rax
    mov %fs, %rax
    push %rax
    mov %gs, %rax
    push %rax
    mov %cr2, %rdi
    push %rdi
.endm

.macro restore_context
    add $24, %rsp // skip cr2, gs and fs
    pop %rax
    mov %rax, %es
    pop %rax
    mov %rax, %ds
    pop %rax
    pop %rbx
    pop %rcx
    pop %rdx
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
.endm

.section .text

isr_no_err_stub 0
isr_no_err_stub 1
isr_no_err_stub 2
isr_no_err_stub 3
isr_no_err_stub 4
isr_no_err_stub 5
isr_no_err_stub 6
isr_no_err_stub 7
isr_err_stub    8
isr_no_err_stub 9
isr_err_stub    10
isr_err_stub    11
isr_err_stub    12
isr_err_stub    13
isr_err_stub    14
isr_no_err_stub 15
isr_no_err_stub 16
isr_err_stub    17
isr_no_err_stub 18
isr_no_err_stub 19
isr_no_err_stub 20
isr_no_err_stub 21
isr_no_err_stub 22
isr_no_err_stub 23
isr_no_err_stub 24
isr_no_err_stub 25
isr_no_err_stub 26
isr_no_err_stub 27
isr_no_err_stub 28
isr_no_err_stub 29
isr_err_stub    30
isr_no_err_stub 31
isr_no_err_stub 32
isr_no_err_stub 33
isr_no_err_stub 34
isr_no_err_stub 35
isr_no_err_stub 36
isr_no_err_stub 37
isr_no_err_stub 38
isr_no_err_stub 39
isr_no_err_stub 40
isr_no_err_stub 41
isr_no_err_stub 42
isr_no_err_stub 43
isr_no_err_stub 44
isr_no_err_stub 45
isr_no_err_stub 46
isr_no_err_stub 47
isr_no_err_stub 48
isr_no_err_stub 49
isr_no_err_stub 50
isr_no_err_stub 51
isr_no_err_stub 52
isr_no_err_stub 53
isr_no_err_stub 54
isr_no_err_stub 55
isr_no_err_stub 56
isr_no_err_stub 57
isr_no_err_stub 58
isr_no_err_stub 59
isr_no_err_stub 60
isr_no_err_stub 61
isr_no_err_stub 62
isr_no_err_stub 63
isr_no_err_stub 64
isr_no_err_stub 65
isr_no_err_stub 66
isr_no_err_stub 67
isr_no_err_stub 68
isr_no_err_stub 69
isr_no_err_stub 70
isr_no_err_stub 71
isr_no_err_stub 72
isr_no_err_stub 73
isr_no_err_stub 74
isr_no_err_stub 75
isr_no_err_stub 76
isr_no_err_stub 77
isr_no_err_stub 78
isr_no_err_stub 79
isr_no_err_stub 80
isr_no_err_stub 81
isr_no_err_stub 82
isr_no_err_stub 83
isr_no_err_stub 84
isr_no_err_stub 85
isr_no_err_stub 86
isr_no_err_stub 87
isr_no_err_stub 88
isr_no_err_stub 89
isr_no_err_stub 90
isr_no_err_stub 91
isr_no_err_stub 92
isr_no_err_stub 93
isr_no_err_stub 94
isr_no_err_stub 95
isr_no_err_stub 96
isr_no_err_stub 97
isr_no_err_stub 98
isr_no_err_stub 99
isr_no_err_stub 100
isr_no_err_stub 101
isr_no_err_stub 102
isr_no_err_stub 103
isr_no_err_stub 104
isr_no_err_stub 105
isr_no_err_stub 106
isr_no_err_stub 107
isr_no_err_stub 108
isr_no_err_stub 109
isr_no_err_stub 110
isr_no_err_stub 111
isr_no_err_stub 112
isr_no_err_stub 113
isr_no_err_stub 114
isr_no_err_stub 115
isr_no_err_stub 116
isr_no_err_stub 117
isr_no_err_stub 118
isr_no_err_stub 119
isr_no_err_stub 120
isr_no_err_stub 121
isr_no_err_stub 122
isr_no_err_stub 123
isr_no_err_stub 124
isr_no_err_stub 125
isr_no_err_stub 126
isr_no_err_stub 127
isr_no_err_stub 128
isr_no_err_stub 129
isr_no_err_stub 130
isr_no_err_stub 131
isr_no_err_stub 132
isr_no_err_stub 133
isr_no_err_stub 134
isr_no_err_stub 135
isr_no_err_stub 136
isr_no_err_stub 137
isr_no_err_stub 138
isr_no_err_stub 139
isr_no_err_stub 140
isr_no_err_stub 141
isr_no_err_stub 142
isr_no_err_stub 143
isr_no_err_stub 144
isr_no_err_stub 145
isr_no_err_stub 146
isr_no_err_stub 147
isr_no_err_stub 148
isr_no_err_stub 149
isr_no_err_stub 150
isr_no_err_stub 151
isr_no_err_stub 152
isr_no_err_stub 153
isr_no_err_stub 154
isr_no_err_stub 155
isr_no_err_stub 156
isr_no_err_stub 157
isr_no_err_stub 158
isr_no_err_stub 159
isr_no_err_stub 160
isr_no_err_stub 161
isr_no_err_stub 162
isr_no_err_stub 163
isr_no_err_stub 164
isr_no_err_stub 165
isr_no_err_stub 166
isr_no_err_stub 167
isr_no_err_stub 168
isr_no_err_stub 169
isr_no_err_stub 170
isr_no_err_stub 171
isr_no_err_stub 172
isr_no_err_stub 173
isr_no_err_stub 174
isr_no_err_stub 175
isr_no_err_stub 176
isr_no_err_stub 177
isr_no_err_stub 178
isr_no_err_stub 179
isr_no_err_stub 180
isr_no_err_stub 181
isr_no_err_stub 182
isr_no_err_stub 183
isr_no_err_stub 184
isr_no_err_stub 185
isr_no_err_stub 186
isr_no_err_stub 187
isr_no_err_stub 188
isr_no_err_stub 189
isr_no_err_stub 190
isr_no_err_stub 191
isr_no_err_stub 192
isr_no_err_stub 193
isr_no_err_stub 194
isr_no_err_stub 195
isr_no_err_stub 196
isr_no_err_stub 197
isr_no_err_stub 198
isr_no_err_stub 199
isr_no_err_stub 200
isr_no_err_stub 201
isr_no_err_stub 202
isr_no_err_stub 203
isr_no_err_stub 204
isr_no_err_stub 205
isr_no_err_stub 206
isr_no_err_stub 207
isr_no_err_stub 208
isr_no_err_stub 209
isr_no_err_stub 210
isr_no_err_stub 211
isr_no_err_stub 212
isr_no_err_stub 213
isr_no_err_stub 214
isr_no_err_stub 215
isr_no_err_stub 216
isr_no_err_stub 217
isr_no_err_stub 218
isr_no_err_stub 219
isr_no_err_stub 220
isr_no_err_stub 221
isr_no_err_stub 222
isr_no_err_stub 223
isr_no_err_stub 224
isr_no_err_stub 225
isr_no_err_stub 226
isr_no_err_stub 227
isr_no_err_stub 228
isr_no_err_stub 229
isr_no_err_stub 230
isr_no_err_stub 231
isr_no_err_stub 232
isr_no_err_stub 233
isr_no_err_stub 234
isr_no_err_stub 235
isr_no_err_stub 236
isr_no_err_stub 237
isr_no_err_stub 238
isr_no_err_stub 239
isr_no_err_stub 240
isr_no_err_stub 241
isr_no_err_stub 242
isr_no_err_stub 243
isr_no_err_stub 244
isr_no_err_stub 245
isr_no_err_stub 246
isr_no_err_stub 247
isr_no_err_stub 248
isr_no_err_stub 249
isr_no_err_stub 250
isr_no_err_stub 251
isr_no_err_stub 252
isr_no_err_stub 253
isr_no_err_stub 254
isr_no_err_stub 255

.section .rodata
.globl isr_stub_table
isr_stub_table:
    .quad isr_stub_0
    .quad isr_stub_1
    .quad isr_stub_2
    .quad isr_stub_3
    .quad isr_stub_4
    .quad isr_stub_5
    .quad isr_stub_6
    .quad isr_stub_7
    .quad isr_stub_8
    .quad isr_stub_9
    .quad isr_stub_10
    .quad isr_stub_11
    .quad isr_stub_12
    .quad isr_stub_13
    .quad isr_stub_14
    .quad isr_stub_15
    .quad isr_stub_16
    .quad isr_stub_17
    .quad isr_stub_18
    .quad isr_stub_19
    .quad isr_stub_20
    .quad isr_stub_21
    .quad isr_stub_22
    .quad isr_stub_23
    .quad isr_stub_24
    .quad isr_stub_25
    .quad isr_stub_26
    .quad isr_stub_27
    .quad isr_stub_28
    .quad isr_stub_29
    .quad isr_stub_30
    .quad isr_stub_31
    .quad isr_stub_32
    .quad isr_stub_33
    .quad isr_stub_34
    .quad isr_stub_35
    .quad isr_stub_36
    .quad isr_stub_37
    .quad isr_stub_38
    .quad isr_stub_39
    .quad isr_stub_40
    .quad isr_stub_41
    .quad isr_stub_42
    .quad isr_stub_43
    .quad isr_stub_44
    .quad isr_stub_45
    .quad isr_stub_46
    .quad isr_stub_47
    .quad isr_stub_48
    .quad isr_stub_49
    .quad isr_stub_50
    .quad isr_stub_51
    .quad isr_stub_52
    .quad isr_stub_53
    .quad isr_stub_54
    .quad isr_stub_55
    .quad isr_stub_56
    .quad isr_stub_57
    .quad isr_stub_58
    .quad isr_stub_59
    .quad isr_stub_60
    .quad isr_stub_61
    .quad isr_stub_62
    .quad isr_stub_63
    .quad isr_stub_64
    .quad isr_stub_65
    .quad isr_stub_66
    .quad isr_stub_67
    .quad isr_stub_68
    .quad isr_stub_69
    .quad isr_stub_70
    .quad isr_stub_71
    .quad isr_stub_72
    .quad isr_stub_73
    .quad isr_stub_74
    .quad isr_stub_75
    .quad isr_stub_76
    .quad isr_stub_77
    .quad isr_stub_78
    .quad isr_stub_79
    .quad isr_stub_80
    .quad isr_stub_81
    .quad isr_stub_82
    .quad isr_stub_83
    .quad isr_stub_84
    .quad isr_stub_85
    .quad isr_stub_86
    .quad isr_stub_87
    .quad isr_stub_88
    .quad isr_stub_89
    .quad isr_stub_90
    .quad isr_stub_91
    .quad isr_stub_92
    .quad isr_stub_93
    .quad isr_stub_94
    .quad isr_stub_95
    .quad isr_stub_96
    .quad isr_stub_97
    .quad isr_stub_98
    .quad isr_stub_99
    .quad isr_stub_100
    .quad isr_stub_101
    .quad isr_stub_102
    .quad isr_stub_103
    .quad isr_stub_104
    .quad isr_stub_105
    .quad isr_stub_106
    .quad isr_stub_107
    .quad isr_stub_108
    .quad isr_stub_109
    .quad isr_stub_110
    .quad isr_stub_111
    .quad isr_stub_112
    .quad isr_stub_113
    .quad isr_stub_114
    .quad isr_stub_115
    .quad isr_stub_116
    .quad isr_stub_117
    .quad isr_stub_118
    .quad isr_stub_119
    .quad isr_stub_120
    .quad isr_stub_121
    .quad isr_stub_122
    .quad isr_stub_123
    .quad isr_stub_124
    .quad isr_stub_125
    .quad isr_stub_126
    .quad isr_stub_127
    .quad isr_stub_128
    .quad isr_stub_129
    .quad isr_stub_130
    .quad isr_stub_131
    .quad isr_stub_132
    .quad isr_stub_133
    .quad isr_stub_134
    .quad isr_stub_135
    .quad isr_stub_136
    .quad isr_stub_137
    .quad isr_stub_138
    .quad isr_stub_139
    .quad isr_stub_140
    .quad isr_stub_141
    .quad isr_stub_142
    .quad isr_stub_143
    .quad isr_stub_144
    .quad isr_stub_145
    .quad isr_stub_146
    .quad isr_stub_147
    .quad isr_stub_148
    .quad isr_stub_149
    .quad isr_stub_150
    .quad isr_stub_151
    .quad isr_stub_152
    .quad isr_stub_153
    .quad isr_stub_154
    .quad isr_stub_155
    .quad isr_stub_156
    .quad isr_stub_157
    .quad isr_stub_158
    .quad isr_stub_159
    .quad isr_stub_160
    .quad isr_stub_161
    .quad isr_stub_162
    .quad isr_stub_163
    .quad isr_stub_164
    .quad isr_stub_165
    .quad isr_stub_166
    .quad isr_stub_167
    .quad isr_stub_168
    .quad isr_stub_169
    .quad isr_stub_170
    .quad isr_stub_171
    .quad isr_stub_172
    .quad isr_stub_173
    .quad isr_stub_174
    .quad isr_stub_175
    .quad isr_stub_176
    .quad isr_stub_177
    .quad isr_stub_178
    .quad isr_stub_179
    .quad isr_stub_180
    .quad isr_stub_181
    .quad isr_stub_182
    .quad isr_stub_183
    .quad isr_stub_184
    .quad isr_stub_185
    .quad isr_stub_186
    .quad isr_stub_187
    .quad isr_stub_188
    .quad isr_stub_189
    .quad isr_stub_190
    .quad isr_stub_191
    .quad isr_stub_192
    .quad isr_stub_193
    .quad isr_stub_194
    .quad isr_stub_195
    .quad isr_stub_196
    .quad isr_stub_197
    .quad isr_stub_198
    .quad isr_stub_199
    .quad isr_stub_200
    .quad isr_stub_201
    .quad isr_stub_202
    .quad isr_stub_203
    .quad isr_stub_204
    .quad isr_stub_205
    .quad isr_stub_206
    .quad isr_stub_207
    .quad isr_stub_208
    .quad isr_stub_209
    .quad isr_stub_210
    .quad isr_stub_211
    .quad isr_stub_212
    .quad isr_stub_213
    .quad isr_stub_214
    .quad isr_stub_215
    .quad isr_stub_216
    .quad isr_stub_217
    .quad isr_stub_218
    .quad isr_stub_219
    .quad isr_stub_220
    .quad isr_stub_221
    .quad isr_stub_222
    .quad isr_stub_223
    .quad isr_stub_224
    .quad isr_stub_225
    .quad isr_stub_226
    .quad isr_stub_227
    .quad isr_stub_228
    .quad isr_stub_229
    .quad isr_stub_230
    .quad isr_stub_231
    .quad isr_stub_232
    .quad isr_stub_233
    .quad isr_stub_234
    .quad isr_stub_235
    .quad isr_stub_236
    .quad isr_stub_237
    .quad isr_stub_238
    .quad isr_stub_239
    .quad isr_stub_240
    .quad isr_stub_241
    .quad isr_stub_242
    .quad isr_stub_243
    .quad isr_stub_244
    .quad isr_stub_245
    .quad isr_stub_246
    .quad isr_stub_247
    .quad isr_stub_248
    .quad isr_stub_249
    .quad isr_stub_250
    .quad isr_stub_251
    .quad isr_stub_252
    .quad isr_stub_253
    .quad isr_stub_254
    .quad isr_stub_255

.section .text
.globl _idt_reload
_idt_reload:
    cli
    lidt (%rdi)
    sti
    ret

.globl _interrupt_disable
_interrupt_disable:
    cli
    ret

.globl _interrupt_enable
_interrupt_enable:
    sti
    ret

.globl __isr_common
__isr_common:
    cmpq $0x23, 24(%rsp) 
    jne .L.not_needed.1
    swapgs
.L.not_needed.1:

    save_context
    
    // setup kernel segments
    mov $0x10, %rdi
    mov %rdi, %ds
    mov %rdi, %es

    // save ctx pointer and isr num
    mov %rsp, %rdi // ctx
    mov %rbp, %rsi // isr num

    cld
    call __isr
    cli

    restore_context

    add $8, %rsp // remove error code

    cmpq $0x23, 8(%rsp)
    jne .L.not_needed.2
    swapgs
.L.not_needed.2:
    iretq

