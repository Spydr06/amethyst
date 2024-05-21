#ifndef _LIBSHARD_H
#define _LIBSHARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <assert.h>
#include <stddef.h>

#define DYNARR_INIT_CAPACITY 16

#define shard_dynarr(name, base) struct name {  \
    size_t count, capacity;                     \
    base* items;                                \
}

#define dynarr_append(ctx, arr, item) do {                                                      \
    if((arr)->count >= (arr)->capacity) {                                                       \
        (arr)->capacity = (arr)->capacity == 0 ? DYNARR_INIT_CAPACITY : (arr)->capacity * 2;    \
        (arr)->items = (ctx)->realloc((arr)->items, (arr)->capacity * sizeof(*(arr)->items));   \
        assert((arr)->items != NULL);                                                           \
    }                                                                                           \
    (arr)->items[(arr)->count++] = (item);                                                      \
} while(0)

#define dynarr_free(ctx, arr) do {      \
    if((arr)->items) {                  \
        (ctx)->free((arr)->items);      \
        (arr)->items = NULL;            \
        (arr)->capacity = 0;            \
    }                                   \
} while(0)

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

struct shard_error* shard_get_errors(struct shard_context* context);

struct shard_context {
    void* (*malloc)(size_t size);
    void* (*realloc)(void* ptr, size_t new_size);
    void (*free)(void* ptr);

    struct shard_arena* idents; 
    struct shard_errors errors;

    struct shard_string_list string_literals;
};

int shard_init(struct shard_context* context);
void shard_deinit(struct shard_context* context);

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
    SHARD_TOK_NUMBER,

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
    SHARD_TOK_COLON, // :
    SHARD_TOK_SEMICOLON, // ;
    SHARD_TOK_PERIOD, // .
    SHARD_TOK_ELLIPSE, // ..
    SHARD_TOK_MERGE, // ++
    SHARD_TOK_QUESTIONMARK, // ?
    SHARD_TOK_EXCLAMATIONMARK, // !
    SHARD_TOK_AT, // @
    SHARD_TOK_ADD, // +
    SHARD_TOK_SUB, // -

    // keywords
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
        double number;
    } value;
};

int shard_lex(struct shard_context* ctx, struct shard_source* src, struct shard_token* token);

enum shard_expr_type {
    SHARD_EXPR_IDENT,

    SHARD_EXPR_NUMBER,
    SHARD_EXPR_STRING,
    SHARD_EXPR_TRUE,
    SHARD_EXPR_FALSE
};

struct shard_expr {
    enum shard_expr_type type;
    struct shard_location loc;

    union {
        char* string;
        const char* ident;
        double number;

        struct {
            struct shard_expr* lhs;
            struct shard_expr* rhs;
        } binop;

        struct {
            struct shard_expr* expr;
        } unaryop;
    };
};

int shard_parse(struct shard_context* ctx, struct shard_source* src, struct shard_expr* expr);

void shard_free_expr(struct shard_context* ctx, struct shard_expr* expr);

struct shard_value {
    int a;
};

void shard_dump_token(char* dest, size_t n, const struct shard_token* tok);
ptrdiff_t shard_dump_expr(char* dest, size_t n, const struct shard_expr* expr);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _LIBSHARD_H */

