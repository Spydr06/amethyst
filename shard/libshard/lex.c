#define _LIBSHARD_INTERNAL
#include <libshard.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stddef.h>

#define PATH_MAX 4096

#define LOC_FROM_LEXER(l, w) ((struct shard_location) { \
        .src = (l)->current_loc.src,                    \
        .line = (l)->current_loc.line,                  \
        .width = (w),                                   \
        .offset = (l)->current_loc.offset - (w),        \
        .column = (l)->current_loc.column               \
    })

#define BASIC_TOK(tok, l, type) (                 \
    init_token((tok), (type),                       \
        LOC_FROM_LEXER(l, token_widths[(type)]), \
        (union shard_token_value){.string = NULL}   \
    ))

#define STRING_TOK(tok, l, type, str, loc) (      \
    init_token((tok), (type),                       \
        (loc),                                      \
        (union shard_token_value){.string = (str)}  \
    ))

#define ERR_TOK(tok, l, err) (                    \
    init_token((tok), SHARD_TOK_ERR,                \
        LOC_FROM_LEXER(l, 1),                    \
        (union shard_token_value){.string = (err)}  \
    ))

#define KEYWORD_TOK(tok, l, kind) BASIC_TOK(tok, l, SHARD_TOK_##kind)
#define EOF_TOK(tok, l) KEYWORD_TOK(tok, l, EOF);

struct interpolation_data {
    bool multiline;
    bool is_path;
    int brace_depth;
    int (*is_end_quote)(int);
};

shard_dynarr(interpolation_stack, struct interpolation_data);

struct shard_lexer {
    struct shard_context* ctx;
    struct shard_source* src;

    struct shard_string buffer;
    struct shard_location current_loc;

    struct interpolation_stack interpolation_stack;
    bool handle_interpolation;

    int brace_depth;
};

static char tmpbuf[PATH_MAX + 1];

static const unsigned token_widths[] = {
    0, // eof
    0, // ident
    0, // string
    0, // path
    0, // int
    0, // float
    1, // (
    1, // )
    1, // [
    1, // ]
    1, // {
    1, // }
    1, // =
    2, // ==
    2, // !=
    1, // >
    2, // >=
    1, // <
    2, // <=
    2, // <<
    2, // >>
    1, // :
    2, // ::
    1, // ;
    1, // ,
    1, // .
    3, // ...
    2, // //
    2, // ++
    1, // ?
    1, // !
    1, // @
    1, // +
    1, // -
    1, // *
    1, // /
    1, // |
    2, // &&
    2, // ||
    2, // ->
    2, // =>
    1, // $
    2, // ${
    3, // rec
    2, // or
    2, // if
    4, // then
    4, // else
    6, // assert
    3, // let
    2, // in
    4, // with
    7, // inherit
    4, // case
    2, // of
};

static_assert((sizeof(token_widths) / sizeof(unsigned)) == _SHARD_TOK_LEN);

static const struct {
    const char* keyword;
    enum shard_token_type type;
} keywords[] = {
    {"rec", SHARD_TOK_REC},
    {"or", SHARD_TOK_OR},
    {"if", SHARD_TOK_IF},
    {"then", SHARD_TOK_THEN},
    {"else", SHARD_TOK_ELSE},
    {"assert", SHARD_TOK_ASSERT},
    {"let", SHARD_TOK_LET},
    {"in", SHARD_TOK_IN},
    {"with", SHARD_TOK_WITH},
    {"inherit", SHARD_TOK_INHERIT},
    {"case", SHARD_TOK_CASE},
    {"of", SHARD_TOK_OF},
    {NULL, SHARD_TOK_EOF},
};

static inline struct shard_location default_loc(struct shard_source* src) {
    return (struct shard_location){
        .src = src,
        .line = src->line_offset,
        .offset = 0,
        .width = 0,
        .column = 1,
    };
}

struct shard_lexer* shard_lex_init(struct shard_context* ctx, struct shard_source* src) {
    struct shard_lexer* l = ctx->malloc(sizeof(struct shard_lexer));

    l->ctx = ctx;
    l->src = src;
    l->current_loc = default_loc(src);
    l->brace_depth = 0;
    l->interpolation_stack = (struct interpolation_stack){0};
    l->handle_interpolation = false;
    l->buffer = (struct shard_string){0};

    int err = src->read_all(src, &l->buffer);
    assert(err == 0);

    return l;
}

void shard_lex_free(struct shard_lexer* l) {
    if(l->src->buffer_dtor)
        l->src->buffer_dtor(&l->buffer);

    dynarr_free(l->ctx, &l->interpolation_stack);
    l->ctx->free(l);
}

static char peek_char(struct shard_lexer* l) {
    if(l->current_loc.offset + 1 >= l->buffer.count)
        return EOF;

    char c = l->buffer.items[l->current_loc.offset];
    return c ? c : EOF;
}

static char peek_char_n(struct shard_lexer* l, unsigned off) {
    if(l->current_loc.offset + 1 + off >= l->buffer.count)
        return EOF;

    char c = l->buffer.items[l->current_loc.offset + off];
    return c ? c : EOF;
}

static char next_char(struct shard_lexer* l) {
    if(l->current_loc.offset + 1 >= l->buffer.count)
        return EOF;

    char c = l->buffer.items[l->current_loc.offset];
    
    switch(c) {
        case '\n':
            l->current_loc.line++;
            l->current_loc.column = 0;
            break;
        case '\0':
        case EOF:
            return EOF;
    }

    l->current_loc.offset++;
    l->current_loc.column++;
    return c;
}

static inline int l_offset(struct shard_lexer* l) {
    return l->current_loc.offset;
}

static inline int l_line(struct shard_lexer* l) {
    return l->current_loc.line;
}

static enum shard_token_type get_keyword(const char* word) {
    for(unsigned i = 0; keywords[i].keyword; i++) {
        if(strcmp(keywords[i].keyword, word) == 0)
            return keywords[i].type;
    }

    return SHARD_TOK_IDENT;
}

static inline void init_token(struct shard_token* tok, enum shard_token_type type, struct shard_location loc, union shard_token_value value) {
    tok->type = type;
    tok->location = loc;
    tok->value = value;
}

static inline bool is_ident_char(char c) {
    return isalnum(c) || c == '_' || c == '-';
}

bool shard_is_valid_identifier(const char *ident) {
    for(size_t i = 0; ident[i]; i++)
        if(!is_ident_char(ident[i]))
            return false;

    return true;
}

static int is_path_terminator(int c) {
    return c == EOF || isspace(c) || c == ';' || c == ',' || c == ')' || c == ']' || c == '}';
}

static int lex_ident(struct shard_lexer* l, struct shard_token* token) {
    unsigned start = l_offset(l);

    int c, i = 0;
    while(is_ident_char(c = peek_char(l))) {
        if(i >= (int) LEN(tmpbuf)) {
            ERR_TOK(token, l, "identifier too long");
            return ENAMETOOLONG;
        }

        tmpbuf[i++] = (char) c;

        next_char(l);
    }

    unsigned end = l_offset(l);
    
    tmpbuf[i] = '\0';

    enum shard_token_type type = get_keyword(tmpbuf);
    if(type == SHARD_TOK_IDENT) {
        shard_ident_t ident = shard_get_ident(l->ctx, tmpbuf);

        struct shard_location loc = {
            .src = l->src,
            .line = l_line(l),
            .offset = start,
            .width = end - start - 1
        };

        init_token(token, type, loc, (union shard_token_value){.string = (char*) ident});
    }
    else
        BASIC_TOK(token, l, type);
    return 0;
}

static int lex_number(struct shard_lexer* l, struct shard_token* token) {
    unsigned start = l_offset(l);

    int64_t integer = 0;
    int64_t fractional = 0;
    int64_t divisor = 1;

    bool floating = false;
    int c, i = 0;
    while(isdigit(c = peek_char(l)) || c == '\'' || c == '.') {
        if(i >= (int) LEN(tmpbuf)) {
            ERR_TOK(token, l, "floating literal too long");
            return ENAMETOOLONG;
        }

        switch(c) {
            case '\'':
                break;
            case '.':
                if(floating) {
                    ERR_TOK(token, l, "more than one period (`.`) in floating literal");
                    return EINVAL;
                }
                floating = true;
                break;
            default:
                if(floating) {
                    fractional = fractional * 10 + (c - '0');
                    divisor *= 10;
                }
                else 
                    integer = integer * 10 + (c - '0');
        }

        next_char(l);
    }

    tmpbuf[i] = '\0';
    
    unsigned end = l_offset(l);

    struct shard_location loc = {
        .src = l->src,
        .line = l_line(l),
        .offset = start,
        .width = end - start
    };

    errno = 0;
    if(floating) {
        init_token(token, SHARD_TOK_FLOAT, loc, (union shard_token_value){.floating = ((double) integer) + (((double) fractional) / ((double) divisor)) });
    }
    else {
        init_token(token, SHARD_TOK_INT, loc, (union shard_token_value){.integer = integer});
    }
    return 0;
}

static int is_double_quote(int c) {
    return c == '"';
}

static int is_gt_sign(int c) {
    return c == '>';
}

static inline struct interpolation_data* interpolation_top(struct shard_lexer* l) {
    return l->interpolation_stack.count > 0 ? &l->interpolation_stack.items[l->interpolation_stack.count - 1] : NULL;
}

static inline bool closes_interpolation(struct shard_lexer* l) {
    struct interpolation_data* top = interpolation_top(l);
    return top && top->brace_depth == l->brace_depth;
}

static void push_interpolation(struct shard_lexer* l, bool multiline, int (*is_end_quote)(int), bool is_path) {
    struct interpolation_data data = {
        .brace_depth = l->brace_depth,
        .is_end_quote = is_end_quote,
        .multiline = multiline,
        .is_path = is_path
    };

    dynarr_append(l->ctx, &l->interpolation_stack, data);
}

static int lex_string(struct shard_lexer* l, struct shard_token* token, int (*is_end_quote)(int), bool is_path) {
    unsigned start = l_offset(l) - 1;
    
    struct shard_string str = {0};

    bool escaped = false;
    int c;
    while(!is_end_quote(c = peek_char(l)) || escaped) {
        dynarr_append(l->ctx, &str, c);

        escaped = false;
        switch(c) {
        case '\\':
            escaped = true;
            break;
        case '$':
            if(escaped)
                continue;

            bool interpolate_begin = peek_char_n(l, 1) == '{';
            if(interpolate_begin) {
                str.count--;
                push_interpolation(l, false, is_end_quote, is_path);
                goto break_for;
            }
            break;
        case '\n':
            ERR_TOK(token, l, is_path ? "unterminated path literal before newline" : "unterminated string literal before newline");
            break;
        case EOF:
            dynarr_free(l->ctx, &str);
            ERR_TOK(token, l, is_path ? "unterminated path literal" : "unterminated string literal");
            return EINVAL;
        }

        next_char(l);
    }

    if(!is_path)
        next_char(l);

break_for:
    dynarr_append(l->ctx, &str, '\0');

    dynarr_append(l->ctx, &l->ctx->string_literals, str.items);

    unsigned end = l_offset(l);
    struct shard_location loc = {
        .src = l->src,
        .line = l_line(l),
        .offset = start - 1,
        .width = end - start + 1
    };

    STRING_TOK(token, src, is_path ? SHARD_TOK_PATH : SHARD_TOK_STRING, str.items, loc);
    return 0;
}

static int lex_multiline_string(struct shard_lexer* l, struct shard_token* token) {
    unsigned start = l_offset(l) - 1;
    unsigned start_line = l_line(l);

    struct shard_string str = {0};

    bool escaped[3] = {false, false, false};
    for(;;) {
        escaped[2] = escaped[1];
        escaped[1] = escaped[0];
        escaped[0] = false;
        
        switch(peek_char(l)) {
            case '\'':
                if(escaped[1] || escaped[2])
                    dynarr_append(l->ctx, &str, '\'');
                else if(peek_char_n(l, 1) == '\'') {
                    next_char(l);
                    next_char(l);
                    goto break_loop;
                }
                break;
            case '\n':
                dynarr_append(l->ctx, &str, '\n');
                break;
            case '\\':
                escaped[0] = true;
                dynarr_append(l->ctx, &str, '\\');
                break;
            case '$':
                if(escaped[2] || peek_char_n(l, 1) != '{')
                    continue;
                
                push_interpolation(l, true, NULL, false);
                goto break_loop;
            case EOF:
                dynarr_free(l->ctx, &str);
                ERR_TOK(token, l, "unterminated multiline string literal, expect `''`");
                return EINVAL;
            default:
                dynarr_append(l->ctx, &str, peek_char(l));
        }

        next_char(l);
    }

break_loop:

    dynarr_append(l->ctx, &str, '\0');
    dynarr_append(l->ctx, &l->ctx->string_literals, str.items);

    unsigned end = l_offset(l);
    struct shard_location loc = {
        .src = l->src,
        .line = start_line,
        .offset = start - 1,
        .width = end - start + 1
    };

    STRING_TOK(token, src, SHARD_TOK_STRING, str.items, loc);
    return 0;
}

static int pop_interpolation(struct shard_lexer* l, struct shard_token* token) {
    assert(interpolation_top(l) != NULL);

    struct interpolation_data top = *interpolation_top(l);
    l->interpolation_stack.count--; // pop interpolation data
    l->handle_interpolation = false;

    if(top.multiline)
        return lex_multiline_string(l, token);
    else
        return lex_string(l, token, top.is_end_quote, top.is_path);
}

static void skip_whitespace(struct shard_lexer* l) {
    while(isspace(peek_char(l)))
        next_char(l);
}

int shard_lex(struct shard_lexer* l, struct shard_token* token) {
    if(l->handle_interpolation)
        return pop_interpolation(l, token);

repeat:
    skip_whitespace(l);

    switch(peek_char(l)) {
        case EOF:
            EOF_TOK(token, l);
            break;
        case '#':
            char c;
            while((c = next_char(l)) != '\n' && c != EOF);
            goto repeat;
        case '(':
            next_char(l);
            KEYWORD_TOK(token, l, LPAREN);
            break;
        case ')':
            next_char(l);
            KEYWORD_TOK(token, l, RPAREN);
            break;
        case '[':
            next_char(l);
            KEYWORD_TOK(token, l, LBRACKET);
            break;
        case ']':
            next_char(l);
            KEYWORD_TOK(token, l, RBRACKET);
            break;
        case '{':
            next_char(l);
            l->brace_depth++;
            KEYWORD_TOK(token, l, LBRACE);
            break;
        case '}':
            next_char(l);
            l->brace_depth--;
            if(closes_interpolation(l))
                l->handle_interpolation = true;
            KEYWORD_TOK(token, l, RBRACE);
            break;
        case '+':
            next_char(l);
            if(peek_char(l) == '+') {
                next_char(l);
                KEYWORD_TOK(token, l, CONCAT);
            }
            else
                KEYWORD_TOK(token, l, ADD);
            break;
        case '-':
            next_char(l);
            if(peek_char(l) == '>') {
                next_char(l);
                KEYWORD_TOK(token, l, LOGIMPL);
            }
            else
                KEYWORD_TOK(token, l, SUB);
            break;
        case '*':
            next_char(l);
            KEYWORD_TOK(token, l, MUL);
            break;
        case '/':
            if(peek_char_n(l, 1) == '/') {
                next_char(l);
                next_char(l);
                KEYWORD_TOK(token, l, MERGE);
            }
            else if(peek_char_n(l, 1) == '*') {
                for(;;) {
                    while((c = next_char(l)) != '*') {
                        if(c == EOF) {
                            ERR_TOK(token, l, "unterminated multiline comment, expect `*/`");
                            return EOVERFLOW;
                        }
                    }
                    if((c = next_char(l)) == '/')
                        goto repeat;
                }
            }
            else if(!isspace(peek_char_n(l, 1))) {
                return lex_string(l, token, is_path_terminator, true);
            }
            else {
                next_char(l);
                KEYWORD_TOK(token, l, DIV);
            }
            break;
        case '~':
            return lex_string(l, token, is_path_terminator, true);
        case ':':
            next_char(l);
            if(peek_char(l) == ':') {
                next_char(l);
                KEYWORD_TOK(token, l, DOUBLE_COLON);
            }
            else
                KEYWORD_TOK(token, l, COLON);
            break;
        case ';':
            next_char(l);
            KEYWORD_TOK(token, l, SEMICOLON);
            break;
        case '&':
            next_char(l);
            if(peek_char(l) == '&') {
                next_char(l);
                KEYWORD_TOK(token, l, LOGAND);
            }
            else {
                ERR_TOK(token, l, "unknown token `&`, did you mean `&&`?");
                return EINVAL;
            }
            break;
        case '|':
            next_char(l);
            if(peek_char(l) == '|') {
                next_char(l);
                KEYWORD_TOK(token, l, LOGOR);
            }
            else
                KEYWORD_TOK(token, l, PIPE);
            break;
        case ',':
            next_char(l);
            KEYWORD_TOK(token, l, COMMA);
            break;
        case '.': {
            if(peek_char_n(l, 1) == '.' && peek_char_n(l, 2) == '.') {
                next_char(l);
                next_char(l);
                next_char(l);
                KEYWORD_TOK(token, l, ELLIPSE);
            }
            else if(peek_char_n(l, 1) == '.' && peek_char_n(l, 2) == '/') {
                return lex_string(l, token, is_path_terminator, true);
            }
            else if(peek_char_n(l, 1) == '/') {
                return lex_string(l, token, is_path_terminator, true);
            }
            else {
                next_char(l);
                KEYWORD_TOK(token, l, PERIOD);
            }
        } break;
        case '?':
            next_char(l);
            KEYWORD_TOK(token, l, QUESTIONMARK);
            break;
        case '!':
            next_char(l);
            if(peek_char(l) == '=') {
                next_char(l);
                KEYWORD_TOK(token, l, NE);
            }
            else
                KEYWORD_TOK(token, l, EXCLAMATIONMARK);
            break;
        case '>':
            next_char(l);
            if(peek_char(l) == '=') {
                next_char(l);
                KEYWORD_TOK(token, l, GE);
            }
            else if(peek_char(l) == '>') {
                next_char(l);
                KEYWORD_TOK(token, l, RCOMPOSE);
            }
            else
                KEYWORD_TOK(token, l, GT);
            break;
        case '<':
            next_char(l);
            if(peek_char(l) == '=') {
                next_char(l);
                KEYWORD_TOK(token, l, LE);


            }
            else if(peek_char(l) == '<') {
                next_char(l);
                KEYWORD_TOK(token, l, LCOMPOSE);
            }
            else if(!isspace(peek_char(l))) {
                int err = lex_string(l, token, is_gt_sign, false);
                token->type = SHARD_TOK_PATH;
                return err;
            }
            else
                KEYWORD_TOK(token, l, LT);
            break;
        case '@':
            next_char(l);
            KEYWORD_TOK(token, l, AT);
            break;
        case '$':
            next_char(l);
            if(peek_char(l) == '{') {
                next_char(l);
                l->brace_depth++;
                KEYWORD_TOK(token, l, INTERPOLATION);
            }
            else
                KEYWORD_TOK(token, l, DOLLAR);
            break;
        case '=':
            next_char(l);
            if(peek_char(l) == '=') {
                next_char(l);
                KEYWORD_TOK(token, l, EQ);
            }
            else if(peek_char(l) == '>') {
                next_char(l);
                KEYWORD_TOK(token, l, ARROW);
            }
            else
                KEYWORD_TOK(token, l, ASSIGN);
            break;
        case '\'':
            next_char(l);
            if(next_char(l) == '\'')
                return lex_multiline_string(l, token);
            else {
                ERR_TOK(token, l, "unexpected character, expect second `'` for multiline string");
                return EINVAL;
            }
        case '\"':
            next_char(l);
            return lex_string(l, token, is_double_quote, false);
        default:
            if(isdigit(peek_char(l))) {
                return lex_number(l, token);
            }
            else if(is_ident_char(peek_char(l))) {
                return lex_ident(l, token);
            }
            else
                ERR_TOK(token, l, "Unexpected character");
            return EINVAL;
    }

    return 0;
}

