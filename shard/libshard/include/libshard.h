#ifndef _LIBSHARD_H
#define _LIBSHARD_H

/*
    libshard.h - Library for the shard programming language interpreter.

    The MIT License (MIT)

    Copyright (c) 2024 Spydr06

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <setjmp.h>

#define __SHARD_QUOTE(arg) #arg
#define __SHARD_TOSTRING(arg) __SHARD_QUOTE(arg)

#define SHARD_VERSION_X 0
#define SHARD_VERSION_Y 1
#define SHARD_VERSION_Z 0

#define SHARD_VERSION __SHARD_TOSTRING(SHARD_VERSION_X) "." __SHARD_TOSTRING(SHARD_VERSION_Y) "." __SHARD_TOSTRING(SHARD_VERSION_Z)

#define DYNARR_INIT_CAPACITY 16

#define __shard_dynarr(name, base, ct) struct name {    \
    ct count, capacity;                                 \
    base* items;                                        \
}

#define shard_dynarr(name, base) __shard_dynarr(name, base, size_t)

#ifdef _LIBSHARD_INTERNAL
    #define SHARD_DECL __attribute__((visibility("default"))) 
#else
    #define SHARD_DECL
#endif

#ifdef _LIBSHARD_VALGRIND_DEBUGGING
    //
    // Valgrind has issues with glibc's builtin `memset` due to aggressive optimization on some platforms.
    // Therefore, we need to provide a custom alternative that is not optimized.
    // 
    // !! ONLY USE THIS FOR DEBUGGING !!
    //

    #define memset __libshard_valgrind_memset

    static inline void* memset(void* s, int c, size_t n) {
        char* bytes = (char*) s;
        for(size_t i = 0; i < n; i++)
            bytes[i] = (char) c;
        return s;
    }
#endif

struct shard_alloc_map;
struct shard_arena;
struct shard_builtin;
struct shard_context;
struct shard_evaluator;
struct shard_expr;
struct shard_hashmap;
struct shard_lazy_value;
struct shard_list;
struct shard_open_source;
struct shard_pattern;
struct shard_set;
struct shard_source;
struct shard_token;
struct shard_value;

typedef const char* shard_ident_t;

struct shard_location {
    unsigned line, column, width, offset;
    struct shard_source* src;
};

struct shard_arena {
    uint8_t *region;
    size_t size, current;
    struct shard_arena* next;
};

SHARD_DECL struct shard_arena* shard_arena_init(const struct shard_context* ctx);
SHARD_DECL void* shard_arena_malloc(const struct shard_context* ctx, struct shard_arena* arena, size_t size);
SHARD_DECL void shard_arena_free(const struct shard_context* ctx, struct shard_arena* arena);

struct shard_hashpair {
    const char* key;
    void* value;
};

struct shard_hashmap {
    size_t size;
    size_t alloc;
    struct shard_hashpair* pairs;
};

SHARD_DECL void shard_hashmap_init(const struct shard_context* ctx, struct shard_hashmap* map, size_t init_size);
SHARD_DECL void shard_hashmap_free(const struct shard_context* ctx, struct shard_hashmap* map);

SHARD_DECL int shard_hashmap_put(const struct shard_context* ctx, struct shard_hashmap* map, const char* key, void* value);
SHARD_DECL void* shard_hashmap_get(const struct shard_hashmap* map, const char* key);

enum shard_gc_backend {
    SHARD_GC_BOEHM,
    SHARD_GC_BUILTIN
};

extern const enum shard_gc_backend _shard_gc_backend;

struct shard_gc;

SHARD_DECL struct shard_gc* shard_gc_begin(struct shard_context* ctx, void* stack_base);
SHARD_DECL void shard_gc_end(struct shard_gc* gc);

SHARD_DECL void shard_gc_pause(volatile struct shard_gc* gc);
SHARD_DECL void shard_gc_resume(volatile struct shard_gc* gc);
SHARD_DECL void shard_gc_run(volatile struct shard_gc* gc);
SHARD_DECL void* shard_gc_make_static(volatile struct shard_gc* gc, void* ptr);

SHARD_DECL void* shard_gc_malloc(volatile struct shard_gc* gc, size_t size);
SHARD_DECL void* shard_gc_calloc(volatile struct shard_gc* gc, size_t nmemb, size_t size);
SHARD_DECL void* shard_gc_realloc(volatile struct shard_gc* gc, void* ptr, size_t size);
SHARD_DECL void shard_gc_free(volatile struct shard_gc* gc, void* ptr);

SHARD_DECL char* shard_gc_strdup(volatile struct shard_gc* gc, const char* str, size_t size);

struct shard_error {
    struct shard_location loc;
    char* err;
    bool heap;
    int _errno;
};

struct shard_error* shard_get_errors(struct shard_context* context);
size_t shard_get_num_errors(struct shard_context* context);

void shard_remove_errors(struct shard_context* context);

#define SHARD_DYNARR_EMPTY {0, 0, 0}

shard_dynarr(shard_errors, struct shard_error);
shard_dynarr(shard_string, char);
shard_dynarr(shard_string_list, char*);
shard_dynarr(shard_expr_list, struct shard_expr);
shard_dynarr(shard_pattern_list, struct shard_pattern);

SHARD_DECL void shard_string_append(struct shard_context* ctx, struct shard_string* str, const char* str2);
SHARD_DECL void shard_string_appendn(struct shard_context* ctx, struct shard_string* str, const char* str2, size_t size);
SHARD_DECL void shard_string_push(struct shard_context* ctx, struct shard_string* str, char c);
SHARD_DECL void shard_string_free(struct shard_context* ctx, struct shard_string* str);

SHARD_DECL void shard_gc_string_append(volatile struct shard_gc* gc, struct shard_string* str, const char* str2);
SHARD_DECL void shard_gc_string_appendn(volatile struct shard_gc* gc, struct shard_string* str, const char* str2, size_t size);
SHARD_DECL void shard_gc_string_push(volatile struct shard_gc* ctx, struct shard_string* str, char c);
SHARD_DECL void shard_gc_string_free(volatile struct shard_gc* ctx, struct shard_string* str);

struct shard_scope {
    struct shard_scope* outer;
    struct shard_set* bindings;
};

struct shard_context {
    void* (*malloc)(size_t size);
    void* (*realloc)(void* ptr, size_t new_size);
    void (*free)(void* ptr);
    char* (*realpath)(const char* restrict path, char* restrict resolved_path);
    char* (*dirname)(char* path);
    int (*access)(const char* path, int mode);

    int R_ok, W_ok, X_ok;

    int (*open)(const char* path, struct shard_source* dest, const char* restrict mode);

    const char* current_system;
    const char* home_dir;

    struct shard_hashmap open_sources;

    struct shard_arena* ast;
    struct shard_string_list include_dirs;

    struct shard_arena* ident_arena;
    struct shard_hashmap idents;

    struct shard_errors errors;

    struct shard_string_list string_literals;

    struct shard_gc* gc;

    bool builtin_initialized;
    bool ffi_initialized;
    struct shard_scope builtin_scope;

    void* userp;
};

#define shard_init(ctx) shard_init_ext((ctx), __builtin_frame_address(0))

SHARD_DECL int shard_init_ext(struct shard_context* ctx, void* stack_base);
SHARD_DECL void shard_include_dir(struct shard_context* ctx, char* path);

SHARD_DECL void shard_set_current_system(struct shard_context* ctx, const char* current_system);

SHARD_DECL int shard_define(struct shard_context* ctx, shard_ident_t ident, struct shard_lazy_value* value);
SHARD_DECL int shard_define_builtin(struct shard_context* ctx, struct shard_builtin* builtin);
SHARD_DECL int shard_define_constant(struct shard_context* ctx, shard_ident_t ident, struct shard_value value);

SHARD_DECL void shard_deinit(struct shard_context* ctx);

SHARD_DECL shard_ident_t shard_get_ident(struct shard_context* ctx, const char* ident);

struct shard_source {
    void* userp;
    const char* origin;

    int (*read_all)(struct shard_source* self, struct shard_string* dest);
    int (*close)(struct shard_source* self);

    void (*buffer_dtor)(struct shard_string* str);

    unsigned line_offset;
};

SHARD_DECL int shard_eval(struct shard_context* ctx, struct shard_open_source* source);
SHARD_DECL int shard_eval_lazy(struct shard_context* ctx, struct shard_lazy_value* value);
SHARD_DECL struct shard_value shard_eval_call(volatile struct shard_evaluator* e, struct shard_value value, struct shard_lazy_value* arg, struct shard_location loc);
SHARD_DECL struct shard_value shard_eval_lazy2(volatile struct shard_evaluator* e, struct shard_lazy_value* value);

SHARD_DECL struct shard_value shard_eval_addition(volatile struct shard_evaluator* e, struct shard_value left, struct shard_value right, struct shard_location* loc);
SHARD_DECL struct shard_value shard_eval_subtraction(volatile struct shard_evaluator* e, struct shard_value left, struct shard_value right, struct shard_location* loc);
SHARD_DECL struct shard_value shard_eval_multiplication(volatile struct shard_evaluator* e, struct shard_value left, struct shard_value right, struct shard_location* loc);
SHARD_DECL struct shard_value shard_eval_division(volatile struct shard_evaluator* e, struct shard_value left, struct shard_value right, struct shard_location* loc);

SHARD_DECL void shard_update_set_scopes(volatile struct shard_evaluator* e, struct shard_set* new, struct shard_set* old);

SHARD_DECL int shard_call(struct shard_context* ctx, struct shard_value func, struct shard_value* arg, struct shard_value* result);

SHARD_DECL int shard_serialize(struct shard_context* ctx, struct shard_string* dest, struct shard_value value);
SHARD_DECL void shard_serialize2(volatile struct shard_evaluator* e, struct shard_string* dest, struct shard_value value);

enum shard_token_type {
    SHARD_TOK_EOF = 0,

    // primitives
    SHARD_TOK_IDENT,
    SHARD_TOK_STRING,
    SHARD_TOK_PATH,
    SHARD_TOK_INT,
    SHARD_TOK_FLOAT,

    // symbols
    SHARD_TOK_LPAREN, // (
    SHARD_TOK_RPAREN, // )
    SHARD_TOK_LBRACKET, // [
    SHARD_TOK_RBRACKET, // ]
    SHARD_TOK_LBRACE, // {
    SHARD_TOK_RBRACE, // }
    SHARD_TOK_ASSIGN, // =
    SHARD_TOK_EQ, // ==
    SHARD_TOK_NE, // !=
    SHARD_TOK_GT, // >
    SHARD_TOK_GE, // >=
    SHARD_TOK_LT, // <
    SHARD_TOK_LE, // <=
    SHARD_TOK_LCOMPOSE, // <<
    SHARD_TOK_RCOMPOSE, // >>
    SHARD_TOK_COLON, // :
    SHARD_TOK_DOUBLE_COLON, // ::
    SHARD_TOK_SEMICOLON, // ;
    SHARD_TOK_COMMA, // ,
    SHARD_TOK_PERIOD, // .
    SHARD_TOK_ELLIPSE, // ...
    SHARD_TOK_MERGE, // //
    SHARD_TOK_CONCAT, // ++
    SHARD_TOK_QUESTIONMARK, // ?
    SHARD_TOK_EXCLAMATIONMARK, // !
    SHARD_TOK_AT, // @
    SHARD_TOK_ADD, // +
    SHARD_TOK_SUB, // -
    SHARD_TOK_MUL, // *
    SHARD_TOK_DIV, // /
    SHARD_TOK_PIPE, // |
    SHARD_TOK_LOGAND, // &&
    SHARD_TOK_LOGOR, // ||
    SHARD_TOK_LOGIMPL, // ->
    SHARD_TOK_ARROW, // =>
    SHARD_TOK_DOLLAR, // $
    SHARD_TOK_INTERPOLATION, // ${

    // keywords
    SHARD_TOK_REC,
    SHARD_TOK_OR,
    SHARD_TOK_IF,
    SHARD_TOK_THEN,
    SHARD_TOK_ELSE,
    SHARD_TOK_ASSERT,
    SHARD_TOK_LET,
    SHARD_TOK_IN,
    SHARD_TOK_WITH,
    SHARD_TOK_INHERIT,
    SHARD_TOK_CASE,
    SHARD_TOK_OF,

    _SHARD_TOK_LEN,
    SHARD_TOK_ERR = -1
};

struct shard_token {
    enum shard_token_type type;
    struct shard_location location;

    union shard_token_value {
        char* string;
        double floating;
        int64_t integer;
    } value;
};

struct shard_lexer;

SHARD_DECL struct shard_lexer* shard_lex_init(struct shard_context* ctx, struct shard_source* src);
SHARD_DECL void shard_lex_free(struct shard_lexer* l);

SHARD_DECL int shard_lex(struct shard_lexer* l, struct shard_token* token);

SHARD_DECL bool shard_is_valid_identifier(const char *ident);

shard_dynarr(shard_attr_path, shard_ident_t);

struct shard_binding {
    struct shard_location loc;
    shard_ident_t ident;
    struct shard_expr* value;
};

shard_dynarr(shard_binding_list, struct shard_binding);

enum shard_expr_type {
    SHARD_EXPR_IDENT = 1,

    SHARD_EXPR_INT,
    SHARD_EXPR_FLOAT,
    SHARD_EXPR_STRING,
    SHARD_EXPR_PATH,

    SHARD_EXPR_INTERPOLATED_STRING,
    SHARD_EXPR_INTERPOLATED_PATH,

    SHARD_EXPR_NOT,
    SHARD_EXPR_NEGATE,
    SHARD_EXPR_TERNARY,
    SHARD_EXPR_CASE_OF,

    SHARD_EXPR_ADD,
    SHARD_EXPR_SUB,
    SHARD_EXPR_MUL,
    SHARD_EXPR_DIV,
    SHARD_EXPR_EQ,
    SHARD_EXPR_NE,
    SHARD_EXPR_GT,
    SHARD_EXPR_GE,
    SHARD_EXPR_LT,
    SHARD_EXPR_LE,
    SHARD_EXPR_MERGE,
    SHARD_EXPR_CONCAT,
    SHARD_EXPR_ATTR_TEST,
    SHARD_EXPR_ATTR_SEL,
    SHARD_EXPR_LOGOR,
    SHARD_EXPR_LOGAND,
    SHARD_EXPR_LOGIMPL,
    SHARD_EXPR_LCOMPOSE,
    SHARD_EXPR_RCOMPOSE,

    SHARD_EXPR_CALL,
    SHARD_EXPR_WITH,
    SHARD_EXPR_ASSERT,

    SHARD_EXPR_LIST,
    SHARD_EXPR_SET,
    SHARD_EXPR_FUNCTION,
    SHARD_EXPR_LET,

    SHARD_EXPR_BUILTIN,
};

SHARD_DECL void shard_attr_path_init(struct shard_context* ctx, struct shard_attr_path* path);
SHARD_DECL void shard_attr_path_gc_init(struct shard_gc* gc, struct shard_attr_path* path);

SHARD_DECL void shard_attr_path_append(struct shard_context* ctx, struct shard_attr_path* path, shard_ident_t ident);
SHARD_DECL void shard_attr_path_gc_append(struct shard_gc* gc, struct shard_attr_path* path, shard_ident_t ident);

struct shard_expr {
    enum shard_expr_type type;
    struct shard_location loc;

    union {
        char* string;
        const char* ident;
        double floating;
        int64_t integer;

        struct {
            struct shard_expr* lhs;
            struct shard_expr* rhs;
        } binop;

        struct {
            struct shard_expr* expr;
        } unaryop;

        struct {
            struct shard_expr* cond;
            struct shard_expr* if_branch;
            struct shard_expr* else_branch;
        } ternary;
        
        struct {
            struct shard_expr* cond;
            struct shard_expr_list branches;
            struct shard_pattern_list patterns;
        } case_of;

        struct {
            struct shard_expr_list elems;
        } list;

        struct {
            bool recursive;
            struct shard_hashmap attrs;
        } set;

        struct {
            struct shard_expr* set;
            struct shard_attr_path path;
        } attr_test;

        struct {
            struct shard_expr* set;
            struct shard_attr_path path;
            struct shard_expr* default_value;
        } attr_sel;

        struct {
            struct shard_pattern* arg;
            struct shard_expr* body;
        } func;

        struct {
            struct shard_binding_list bindings;
            struct shard_expr* expr;
        } let;

        struct {
            struct shard_builtin* builtin;
        } builtin;

        struct {
            struct shard_string_list strings;
            struct shard_expr_list exprs;
        } interpolated;
    };
};

enum shard_value_type {
    SHARD_VAL_NULL     = 1 << 0,
    SHARD_VAL_BOOL     = 1 << 1,
    SHARD_VAL_INT      = 1 << 2,
    SHARD_VAL_FLOAT    = 1 << 3,
    SHARD_VAL_STRING   = 1 << 4,
    SHARD_VAL_PATH     = 1 << 5,
    SHARD_VAL_LIST     = 1 << 6,
    SHARD_VAL_SET      = 1 << 7,
    SHARD_VAL_FUNCTION = 1 << 8,
    SHARD_VAL_BUILTIN  = 1 << 9
};

#define SHARD_VAL_CALLABLE (SHARD_VAL_FUNCTION | SHARD_VAL_BUILTIN | SHARD_VAL_SET)
#define SHARD_VAL_ANY (SHARD_VAL_NULL | SHARD_VAL_BOOL | SHARD_VAL_INT | SHARD_VAL_FLOAT | SHARD_VAL_STRING | SHARD_VAL_PATH | SHARD_VAL_LIST | SHARD_VAL_SET | SHARD_VAL_FUNCTION | SHARD_VAL_BUILTIN)
#define SHARD_VAL_NUMERIC (SHARD_VAL_INT | SHARD_VAL_FLOAT)
#define SHARD_VAL_TEXTUAL (SHARD_VAL_STRING | SHARD_VAL_PATH)

const char* shard_value_type_to_string(struct shard_context* ctx, enum shard_value_type type);

enum shard_pattern_type {
    SHARD_PAT_IDENT,
    SHARD_PAT_SET,
    SHARD_PAT_CONSTANT
};

struct shard_pattern {
    enum shard_pattern_type type;
    struct shard_location loc;

    bool ellipsis; 
    enum shard_value_type type_constraint;

    struct shard_binding_list attrs; 
    shard_ident_t ident;

    struct shard_expr* constant;
    struct shard_expr* condition;
};

SHARD_DECL int shard_parse(struct shard_context* ctx, struct shard_source* src, struct shard_expr* expr);

SHARD_DECL void shard_free_expr(struct shard_context* ctx, struct shard_expr* expr);

struct shard_error_scope {
    struct shard_error_scope* prev;
    struct shard_location loc;
};

struct shard_evaluator {
    struct shard_context* ctx;
    struct shard_gc* gc;

    struct shard_scope* scope;
    struct shard_error_scope* error_scope;
    jmp_buf* exception;
};

enum shard_evaluator_status {
    SHARD_EVAL_OK      = 0,
    SHARD_EVAL_ERROR
};

_Noreturn __attribute__((format(printf, 3, 4))) 
SHARD_DECL void shard_eval_throw(volatile struct shard_evaluator* e, struct shard_location loc, const char* fmt, ...);

struct shard_builtin {
    const char* full_ident;
    struct shard_value (*callback)(volatile struct shard_evaluator*, struct shard_builtin*, struct shard_lazy_value**);
    unsigned arity;
    enum shard_value_type* arg_types;
};

#define SHARD_BUILTIN(_ident, _callback, ...) ((struct shard_builtin){                          \
        .full_ident=(_ident),                                                                   \
        .callback=(_callback),                                                                  \
        .arity=sizeof((enum shard_value_type[]){__VA_ARGS__}) / sizeof(enum shard_value_type),  \
        .arg_types=(enum shard_value_type[]){__VA_ARGS__}                                       \
    })

#define SHARD_BUILTIN_CONST(_ident, _callback) ((struct shard_builtin){ \
        .full_ident=(_ident),                                           \
        .callback=(_callback),                                          \
        .arity=0,                                                       \
        .arg_types=NULL                                                 \
    })

struct shard_value {
    enum shard_value_type type;

    union {
        bool boolean;
        int64_t integer;
        double floating;
        
        struct {
            const char* string;
            size_t strlen;
        };

        struct {
            const char* path;
            size_t pathlen;
        };

        struct {
            struct shard_list* head;
        } list;

        struct shard_set* set;

        struct {
            struct shard_pattern* arg;
            struct shard_expr* body;
            struct shard_scope* scope;
        } function;

        struct {
            struct shard_builtin* builtin;
            struct shard_lazy_value** queued_args;
            unsigned num_queued_args;
        } builtin;
    };
};

SHARD_DECL bool shard_values_equal(volatile struct shard_evaluator *e, struct shard_value* lhs, struct shard_value* rhs);

struct shard_lazy_value {
    union {
        struct {
            struct shard_expr* lazy;
            struct shard_scope* scope;
        };
        struct shard_value eval;
    };
    bool evaluated;
};

SHARD_DECL struct shard_lazy_value* shard_lazy(struct shard_context* ctx, struct shard_expr* lazy, struct shard_scope* scope);
SHARD_DECL struct shard_lazy_value* shard_unlazy(struct shard_context* ctx, struct shard_value value);

struct shard_list {
    struct shard_list* next;
    struct shard_lazy_value* value;   
};

SHARD_DECL size_t shard_list_length(struct shard_list* list);

struct shard_set {
    size_t size;
    size_t capacity;
    struct {
        shard_ident_t key;
        struct shard_lazy_value* value;
    } entries[];
};

SHARD_DECL struct shard_set* shard_set_init(struct shard_context* ctx, size_t capacity);
SHARD_DECL struct shard_set* shard_set_from_hashmap(volatile struct shard_evaluator* ctx, struct shard_hashmap* map);
SHARD_DECL struct shard_set* shard_set_merge(struct shard_context* ctx, const struct shard_set* fst, const struct shard_set* snd);

SHARD_DECL void shard_set_put(struct shard_set* set, shard_ident_t attr, struct shard_lazy_value* value);
SHARD_DECL int shard_set_get(struct shard_set* set, shard_ident_t attr, struct shard_lazy_value** value);

SHARD_DECL void shard_get_builtins(struct shard_context* ctx, struct shard_scope* dest);

SHARD_DECL struct shard_value shard_builtin_eval_arg(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args, unsigned i);

SHARD_DECL struct shard_value shard_value_copy(volatile struct shard_evaluator* e, struct shard_value val);
SHARD_DECL int shard_value_to_string(struct shard_context* ctx, struct shard_string* str, const struct shard_value* value, int max_depth);

SHARD_DECL const char* shard_token_type_to_str(enum shard_token_type token_type);

SHARD_DECL void shard_dump_pattern(struct shard_context* ctx, struct shard_string* str, const struct shard_pattern* pattern);
SHARD_DECL void shard_dump_token(char* dest, size_t n, const struct shard_token* tok);
SHARD_DECL void shard_dump_expr(struct shard_context* ctx, struct shard_string* str, const struct shard_expr* expr);

struct shard_open_source {
    struct shard_source source;
    struct shard_expr expr;
    struct shard_value result;    

    bool opened : 1;
    bool parsed : 1;
    bool evaluated : 1;

    bool auto_close : 1; // automatically close on deletion using ctx->close()
    bool auto_free : 1;  // automatically free struct on deletion using ctx->free()
};

SHARD_DECL struct shard_open_source* shard_open(struct shard_context* ctx, const char* origin);
SHARD_DECL int shard_register_open(struct shard_context* ctx, const char* origin, bool is_path, struct shard_open_source* source);

SHARD_DECL int shard_enable_ffi(struct shard_context *ctx);

SHARD_DECL struct shard_value shard_ffi_bind(volatile struct shard_evaluator *e, const char* symbol, void *sym, struct shard_set *type);

#ifdef _LIBSHARD_INTERNAL
    #include <libshard-internal.h>
#endif /* _LIBSHARD_INTERAL */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _LIBSHARD_H */

