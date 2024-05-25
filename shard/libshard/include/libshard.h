#ifndef _LIBSHARD_H
#define _LIBSHARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#define DYNARR_INIT_CAPACITY 16

#define __shard_dynarr(name, base, ct) struct name {    \
    ct count, capacity;                                 \
    base* items;                                        \
}

#define shard_dynarr(name, base) __shard_dynarr(name, base, size_t)

struct shard_context;
struct shard_arena;
struct shard_source;
struct shard_token;
struct shard_expr;
struct shard_value;

struct shard_location {
    unsigned line, width, offset;
    struct shard_source* src;
};

struct shard_error {
    struct shard_location loc;
    char* err;
    bool heap;
    int _errno;
};

shard_dynarr(shard_errors, struct shard_error);
shard_dynarr(shard_string, char);
shard_dynarr(shard_string_list, char*);
shard_dynarr(shard_expr_list, struct shard_expr);

struct shard_error* shard_get_errors(struct shard_context* context);

struct shard_context {
    void* (*malloc)(size_t size);
    void* (*realloc)(void* ptr, size_t new_size);
    void (*free)(void* ptr);
    char* (*realpath)(const char* restrict path, char* restrict resolved_path);
    char* (*dirname)(char* path);
    int (*access)(const char* pathname, int mode);

    const char* home_dir;

    struct shard_arena* idents;
    struct shard_arena* ast;

    struct shard_errors errors;

    struct shard_string_list string_literals;
    struct shard_string_list include_dirs;
};

int shard_init(struct shard_context* ctx);
void shard_include_dir(struct shard_context* ctx, char* path);

void shard_deinit(struct shard_context* ctx);

// TODO: add other sources like memory buffers
struct shard_source {
    void* userp;
    const char* origin;

    int (*getc)(struct shard_source* self);
    int (*ungetc)(int c, struct shard_source* self);
    int (*tell)(struct shard_source* self);

    unsigned line;
};

int shard_eval(struct shard_context* context, struct shard_source* src, struct shard_value* result);

struct shard_arena {
    uint8_t *region;
    size_t size, current;
    struct shard_arena* next;
};

struct shard_arena* arena_init(const struct shard_context* ctx);
void* arena_malloc(const struct shard_context* ctx, struct shard_arena* arena, size_t size);
void arena_free(const struct shard_context* ctx, struct shard_arena* arena);

enum shard_token_type {
    SHARD_TOK_EOF = 0,

    // primitives
    SHARD_TOK_IDENT,
    SHARD_TOK_STRING,
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
    SHARD_TOK_COLON, // :
    SHARD_TOK_SEMICOLON, // ;
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
    SHARD_TOK_LOGAND, // &&
    SHARD_TOK_LOGOR, // ||
    SHARD_TOK_LOGIMPL, // ->

    // keywords
    SHARD_TOK_NULL,
    SHARD_TOK_TRUE,
    SHARD_TOK_FALSE,
    SHARD_TOK_REC,
    SHARD_TOK_OR,
    SHARD_TOK_IF,
    SHARD_TOK_THEN,
    SHARD_TOK_ELSE,
    SHARD_TOK_ASSERT,
    SHARD_TOK_LET,
    SHARD_TOK_IN,
    SHARD_TOK_WITH,

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

int shard_lex(struct shard_context* ctx, struct shard_source* src, struct shard_token* token);

enum shard_expr_type {
    SHARD_EXPR_IDENT,

    SHARD_EXPR_INT,
    SHARD_EXPR_FLOAT,
    SHARD_EXPR_STRING,
    SHARD_EXPR_NULL,
    SHARD_EXPR_TRUE,
    SHARD_EXPR_FALSE,

    SHARD_EXPR_NOT,
    SHARD_EXPR_NEGATE,
    SHARD_EXPR_TERNARY,

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

    SHARD_EXPR_CALL,
    SHARD_EXPR_WITH,
    SHARD_EXPR_ASSERT,

    SHARD_EXPR_LIST,
};

typedef const char* shard_ident_t;

shard_dynarr(shard_attr_path, shard_ident_t);

void shard_attr_path_init(struct shard_context* ctx, struct shard_attr_path* path);

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
            struct shard_expr_list elems;
        } list;

        struct {
            struct shard_expr* set;
            struct shard_attr_path path;
        } attr_test;

        struct {
            struct shard_expr* set;
            struct shard_attr_path path;
            struct shard_expr* default_value;
        } attr_sel;
    };
};

int shard_parse(struct shard_context* ctx, struct shard_source* src, struct shard_expr* expr);

void shard_free_expr(struct shard_context* ctx, struct shard_expr* expr);

struct shard_value {
    int a;
};

const char* shard_token_type_to_str(enum shard_token_type token_type);

void shard_dump_token(char* dest, size_t n, const struct shard_token* tok);
void shard_dump_expr(struct shard_context* ctx, struct shard_string* str, const struct shard_expr* expr);

#ifdef _LIBSHARD_INTERNAL

#include <assert.h>
#include <string.h>

#define dynarr_append(ctx, arr, item) do {                                                      \
    if((arr)->count >= (arr)->capacity) {                                                       \
        (arr)->capacity = (arr)->capacity == 0 ? DYNARR_INIT_CAPACITY : (arr)->capacity * 2;    \
        (arr)->items = (ctx)->realloc((arr)->items, (arr)->capacity * sizeof(*(arr)->items));   \
        assert((arr)->items != NULL);                                                           \
    }                                                                                           \
    (arr)->items[(arr)->count++] = (item);                                                      \
} while(0)

#define dynarr_append_many(ctx, arr, new_items, new_items_count) do {                           \
        if ((arr)->count + (new_items_count) > (arr)->capacity) {                                 \
            if ((arr)->capacity == 0) {                                                          \
                (arr)->capacity = DYNARR_INIT_CAPACITY;                                      \
            }                                                                                   \
            while ((arr)->count + (new_items_count) > (arr)->capacity) {                          \
                (arr)->capacity *= 2;                                                            \
            }                                                                                   \
            (arr)->items = (ctx)->realloc((arr)->items, (arr)->capacity*sizeof(*(arr)->items));     \
            assert((arr)->items != NULL && "Buy more RAM lol");                                 \
        }                                                                                       \
        memcpy((arr)->items + (arr)->count, (new_items), (new_items_count)*sizeof(*(arr)->items)); \
        (arr)->count += (new_items_count);                                                       \
    } while (0)

#define dynarr_free(ctx, arr) do {      \
    if((arr)->items) {                  \
        (ctx)->free((arr)->items);      \
        (arr)->items = NULL;            \
        (arr)->capacity = 0;            \
    }                                   \
} while(0)

#define EITHER(a, b) ((a) ? (a) : (b))
#define LEN(arr) (sizeof((arr)) / sizeof((arr)[0]))

static inline size_t strnlen(const char *s, size_t n)
{
	const char *p = memchr(s, 0, n);
	return p ? (size_t) (p - s) : n;
}

static inline char* shard_mempcpy(void *restrict dst, const void *restrict src, size_t n) {
    return (char *)memcpy(dst, src, n) + n;
}

static inline char* shard_stpncpy(char *restrict dst, const char *restrict src, size_t dsize) {
     size_t  dlen = strnlen(src, dsize);
     return memset(shard_mempcpy(dst, src, dlen), 0, dsize - dlen);
}

#endif /* _LIBSHARD_INTERAL */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _LIBSHARD_H */

