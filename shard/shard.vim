if exists("b:current_syntax")
    finish
endif
let b:current_syntax = "shard"

" Constants

syn keyword shardBoolConsts true false null
hi def link shardBoolConsts Boolean

syn keyword shardPrimitiveTypes bool int float string path list set function any
hi def link shardPrimitiveTypes Type

" Keywords

syn keyword shardConditionalK if then else case of or
hi def link shardConditionalK Conditional

syn keyword shardRecKeywordK rec inherit
hi def link shardRecKeywordK Repeat

syn keyword shardStatementK assert let in with
hi def link shardStatementK Statement

" Symbols

syn match shardParens "[\[\](){}]"
hi def link shardParens Delimiter

syn match shardDelims "[.,;]"
hi def link shardDelims Delimiter

syn match shardOperators "[=!<>:+\-@*/|&$]"
hi def link shardOperators Operator

" Numbers

syn match shardInt "[0-9][0-9']*"
hi def link shardInt Number

syn match shardFloat "[0-9][0-9']*\.[0-9']*"
hi def link shardFloat Float

" Strings

syn match shardEscapedChar "\\." contained
hi def link shardEscapedChar SpecialChar

syn region shardString start=+"+ end=+"+ skip=+\\"+ contains=shardEscapedChar
hi def link shardString String

syn match shardChar "'\\.'"
syn match shardChar "'.'"
hi def link shardChar Character

" Paths

syn region shardPath start="\.*/" end="\ze[\s,;]" skip="\\\s" contains=shardEscapedChar
hi def link shardPath String


" Identifiers

syn match shardIdent "[a-zA-Z_][a-zA-Z0-9_]*"
hi def link shardIdent Normal

syn match shardFunctionArg "\<[a-zA-Z_][a-zA-Z0-9_]*\>\s*\ze:"
hi def link shardFunctionArg Function

" Comments

syntax match shardComment "#.*$" contains=shardComment
syntax region shardComment start="/\*" end="\*/" contains=shardComment
hi def link shardComment Comment

