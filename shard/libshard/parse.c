#define _LIBSHARD_INTERNAL
#include <libshard.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

enum precedence {
    PREC_LOWEST,
    
    PREC_LOGIMPL,
    PREC_LOGOR,
    PREC_LOGAND,
    PREC_EQUALITY,
    PREC_COMPARISON,
    PREC_MERGE,
    PREC_NOT,
    PREC_ADDITION,
    PREC_MULTIPLICATION,
    PREC_CONCATENATION,
    PREC_ATTRIBUTE_TEST,
    PREC_NEGATION,
    PREC_CALL,
    PREC_ATTRIBUTE_SELECTION,
    
    PREC_HIGHEST
};

struct parser {
    struct shard_context* ctx;
    struct shard_source* src;
    struct shard_token token;
};

static int advance(struct parser* p) {
    int err = shard_lex(p->ctx, p->src, &p->token);
    if(err) {
        dynarr_append(p->ctx, &p->ctx->errors, ((struct shard_error){
            .err = p->token.value.string,
            .loc = p->token.location,
            ._errno = err
        }));
    }

    return err;
}

static __attribute__((format(printf, 2, 3)))
int errorf(struct parser* p, const char* fmt, ...) {
    char* buffer = p->ctx->malloc(256);
    
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buffer, 1024, fmt, ap);
    va_end(ap);

    dynarr_append(p->ctx, &p->ctx->errors, ((struct shard_error){
        .err = buffer,
        .heap = true,
        .loc = p->token.location,
        ._errno = EINVAL
    }));

//    advance(p);
    return EINVAL;
}

static inline int any_err(int* errors, size_t len) {
    for(register size_t i = 0; i < len; i++)
         if(errors[i])
             return errors[i];
    return 0;
}

static int consume(struct parser* p, enum shard_token_type token_type) {
    if(p->token.type == token_type)
        return advance(p);

    if(p->token.type == SHARD_TOK_ERR)
        return EINVAL;

    advance(p);
    return errorf(
        p,
        "unexpected token `%s`, expect token `%s`",
        shard_token_type_to_str(p->token.type),
        shard_token_type_to_str(token_type)
    );
}

static int parse_expr(struct parser* p, struct shard_expr* expr, enum precedence prec);

static int parse_escape_code(struct parser* p, const char* ptr, char* c) {
    switch(*ptr) {
        case 'e':
            *c = 33;
            break;
        case 'n':
            *c = '\n';
            break;
        case 'r':
            *c = '\r';
            break;
        case '\\':
            *c = '\\';
            break;
        case 'b':
            *c = '\b';
            break;
        case 't':
            *c = '\t';
            break;
        case '\'':
            *c = '\'';
            break;
        case '"':
            *c = '"';
            break;
        case 'f':
            *c = '\f';
            break;
        case ' ':
            *c = ' ';
            break;
        case '\t':
            *c = '\t';
            break;
        case '\v':
            *c = '\v';
            break;
        case '\n':
            *c = '\n';
            break;
        case '\r':
            *c = '\r';
            break;
        default:
            return errorf(p, "invalid escape code: `\\%c`", *ptr ? *ptr : '0');
    }
    return 0;
}

static int parse_string_lit(struct parser* p, struct shard_expr* expr, enum shard_expr_type type) {
    struct shard_string str = {0};

    int err = 0;
    for(const char* ptr = p->token.value.string; *ptr; ptr++) {
        switch(*ptr) {
            case '\\': {
                char c;
                err = parse_escape_code(p, ++ptr, &c);
                if(err) {
                    dynarr_append(p->ctx, &str, '\\');
                    dynarr_append(p->ctx, &str, *ptr);
                }
                else
                    dynarr_append(p->ctx, &str, c);
            } break;
            case '$':
                break;
            default:
                dynarr_append(p->ctx, &str, *ptr);
        }
    }

    dynarr_append(p->ctx, &str, '\0');

    *expr = (struct shard_expr) {
        .type = type,
        .loc = p->token.location,
        .string = str.items
    };

    return advance(p);
}

static int parse_with(struct parser* p, struct shard_expr* expr) {
    expr->type = SHARD_EXPR_WITH;
    expr->loc = p->token.location;
    
    expr->binop.lhs = shard_arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));
    expr->binop.rhs = shard_arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));

    int err[] = {
        advance(p),
        parse_expr(p, expr->binop.lhs, PREC_LOWEST),
        consume(p, SHARD_TOK_SEMICOLON),
        parse_expr(p, expr->binop.rhs, PREC_LOWEST)
    };

    return any_err(err, LEN(err));
}

static int parse_assert(struct parser* p, struct shard_expr* expr) {
    expr->type = SHARD_EXPR_ASSERT;
    expr->loc = p->token.location;
    
    expr->binop.lhs = shard_arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));
    expr->binop.rhs = shard_arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));

    int err[] = {
        advance(p),
        parse_expr(p, expr->binop.lhs, PREC_LOWEST),
        consume(p, SHARD_TOK_SEMICOLON),
        parse_expr(p, expr->binop.rhs, PREC_LOWEST)
    };

    return any_err(err, LEN(err));
}

static int parse_unary_op(struct parser* p, struct shard_expr* expr, enum shard_expr_type expr_type, enum precedence prec) {
    expr->type = expr_type;
    expr->loc = p->token.location; 
    expr->unaryop.expr = shard_arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));

    int err[] = {
        advance(p),
        parse_expr(p, expr->unaryop.expr, prec),
    };
    
    return any_err(err, LEN(err));
}

static int parse_compound(struct parser* p, struct shard_expr* expr) {
    int err[] = {
        advance(p),
        parse_expr(p, expr, PREC_LOWEST),
        consume(p, SHARD_TOK_RPAREN)
    };

    return any_err(err, LEN(err));
}

static int parse_function(struct parser* p, struct shard_expr* expr, struct shard_pattern* arg) {
    expr->type = SHARD_EXPR_FUNCTION;
    expr->loc = p->token.location;

    expr->func.arg = arg;
    expr->func.body = shard_arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));
    return parse_expr(p, expr->func.body, PREC_LOWEST);
}

static int parse_set_pattern(struct parser* p, struct shard_pattern* pattern, shard_ident_t prefix, bool skip_lbrace, shard_ident_t first_attr) {
    pattern->type = SHARD_PAT_SET;
    pattern->ellipsis = false;
    pattern->ident = prefix;
    pattern->loc = p->token.location;
    pattern->attrs = (struct shard_binding_list){0};

    int err = skip_lbrace ? 0 : advance(p);

    bool first_iter = true;
    while(p->token.type != SHARD_TOK_RBRACE) {
        if(p->token.type == SHARD_TOK_ELLIPSE) {
            pattern->ellipsis = true;
            if(advance(p))
                err = EINVAL;
            break;
        }

        struct shard_binding attr = {
            .ident = first_attr,
            .value = NULL
        };

        if(!first_iter || !first_attr) {
            attr.ident = p->token.value.string;
            if(consume(p, SHARD_TOK_IDENT)) {
                err = EINVAL;
                break;
            }
        }

        first_iter = false;

        if(p->token.type == SHARD_TOK_QUESTIONMARK) {
            if(advance(p))
                err = EINVAL;

            attr.value = shard_arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));
            int err2 = parse_expr(p, attr.value, PREC_LOWEST);
            if(err2)
                err = err2;
        }

        dynarr_append(p->ctx, &pattern->attrs, attr);

        if(p->token.type != SHARD_TOK_RBRACE) {
            int err2 = consume(p, SHARD_TOK_COMMA);
            if(err2) {
                err = err2;
                break;
            }
        }
    }

    if(consume(p, SHARD_TOK_RBRACE))
        err = EINVAL;

    if(p->token.type == SHARD_TOK_AT) {
        if(prefix)
            err = errorf(p, "`@` pattern already applied before the set");
        
        if(advance(p))
            err = EINVAL;

        shard_ident_t ident = p->token.value.string;
        if(consume(p, SHARD_TOK_IDENT))
            return EINVAL;

        pattern->ident = ident;
    }

    return err;
}

static int parse_ident(struct parser* p, struct shard_expr* expr) {
    struct shard_location ident_loc = p->token.location;
    char* ident = p->token.value.string;
    int err = advance(p);

    switch(p->token.type) {
        case SHARD_TOK_COLON: {
            struct shard_pattern* arg = shard_arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_pattern));
            arg->type = SHARD_PAT_IDENT;
            arg->ident = ident;
            arg->loc = ident_loc;

            int err2[] = {
                err,
                advance(p),
                parse_function(p, expr, arg),
            };
            return any_err(err2, LEN(err2));
        } break;
        case SHARD_TOK_AT: {
            struct shard_pattern* arg = shard_arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_pattern));
            int err2[] = {
                err,
                advance(p),
                parse_set_pattern(p, arg, ident, false, NULL),
                consume(p, SHARD_TOK_COLON),
                parse_function(p, expr, arg)
            };
            return any_err(err2, LEN(err2));
        } break;
        default:
            *expr = (struct shard_expr) {
                .type = SHARD_EXPR_IDENT,
                    .loc = ident_loc,
                    .string = ident
            };
            return err;
    }
}

static int parse_list(struct parser* p, struct shard_expr* expr) {
    expr->type = SHARD_EXPR_LIST;
    expr->loc = p->token.location;
    expr->list.elems = (struct shard_expr_list){0};

    int err = advance(p);
    
    struct shard_expr elem;
    while(p->token.type != SHARD_TOK_RBRACKET) {
        if(p->token.type == SHARD_TOK_EOF)
            return errorf(p, "unexpected end of file, expect expression or `]`");

        int err2 = parse_expr(p, &elem, PREC_HIGHEST);
        if(err2)
            err = err2;
        dynarr_append(p->ctx, &expr->list.elems, elem);
    }

    int err3 = advance(p);
    return EITHER(err, err3);
}

static int parse_set_inherit(struct parser* p, struct shard_hashmap* attrs) {
    struct shard_expr* base = NULL;

    int err = advance(p);

    if(p->token.type == SHARD_TOK_LPAREN) {
        base = shard_arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));

        int err2[] = {
            advance(p),
            parse_expr(p, base, PREC_LOWEST),
            consume(p, SHARD_TOK_RPAREN)
        };

        int err3 = any_err(err2, LEN(err2));
        if(err3)
            err = err3;
    }

    while(p->token.type == SHARD_TOK_IDENT) {
        shard_ident_t key = p->token.value.string;
        
        if(shard_hashmap_get(attrs, key))
            err = errorf(p, "attribute `%s` already defined for this set", key);

        struct shard_expr* value = shard_arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));
        if(base) {
            value->type = SHARD_EXPR_ATTR_SEL;
            value->loc = p->token.location;
            value->attr_sel.default_value = NULL;
            value->attr_sel.set = base;

            struct shard_attr_path path = {
                .count = 1,
                .capacity = 1,
                .items = p->ctx->malloc(sizeof(shard_ident_t))
            };
            
            ((shard_ident_t*) path.items)[0] = key;

            value->attr_sel.path = path;
        }
        else {
            value->type = SHARD_EXPR_IDENT,
            value->loc = p->token.location;
            value->ident = key;
        }

        shard_hashmap_put(p->ctx, attrs, key, value);

        if(advance(p))
            err = EINVAL;
    }

    return err;
}

static int parse_set(struct parser* p, struct shard_expr* expr, bool recursive) {
    expr->type = SHARD_EXPR_SET;
    expr->loc = p->token.location;
    expr->set.recursive = recursive;
    shard_hashmap_init(p->ctx, &expr->set.attrs, 8);
    
    if(recursive)
        advance(p);

    int err = consume(p, SHARD_TOK_LBRACE);

    bool first_iter = true;
    while(p->token.type != SHARD_TOK_RBRACE) {
        if(p->token.type == SHARD_TOK_EOF)
            return errorf(p, "unexpected end of file, expect set attribute or `]`");

        if(p->token.type == SHARD_TOK_INHERIT) {
            int err2[] = {
                parse_set_inherit(p, &expr->set.attrs),
                consume(p, SHARD_TOK_SEMICOLON)
            };

            int err3 = any_err(err2, LEN(err2));
            if(err3)
                err = err3;
            first_iter = false;
            continue;
        }

        if(first_iter && p->token.type == SHARD_TOK_ELLIPSE) {
            shard_hashmap_free(p->ctx, &expr->set.attrs);

            struct shard_pattern* arg = shard_arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_pattern));
            int err2[] = {
                err,
                parse_set_pattern(p, arg, NULL, true, NULL),
                consume(p, SHARD_TOK_COLON),
                parse_function(p, expr, arg)
            };
            return any_err(err2, LEN(err2));
        }

        shard_ident_t key = p->token.value.string;

        if(consume(p, SHARD_TOK_IDENT))
            err = EINVAL;
        
        if(first_iter && (p->token.type == SHARD_TOK_COMMA || p->token.type == SHARD_TOK_QUESTIONMARK)) {
            shard_hashmap_free(p->ctx, &expr->set.attrs);

            struct shard_pattern* arg = shard_arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_pattern));
            int err2[] = {
                err,
                parse_set_pattern(p, arg, NULL, true, key),
                consume(p, SHARD_TOK_COLON),
                parse_function(p, expr, arg)
            };
            return any_err(err2, LEN(err2));
        }

        first_iter = false;

        struct shard_expr* set = expr;
        struct shard_expr* value = shard_arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));

        while(p->token.type == SHARD_TOK_PERIOD) {
            err = advance(p);
            if(err)
                break;

            struct shard_expr* attr = shard_hashmap_get(&set->set.attrs, key);
            if(attr) {
                set = attr;
                if(attr->type != SHARD_EXPR_SET) {
                    err = errorf(p, "set attribute `%s` is not a set itself", key);
                    break;
                }
            }
            else {
                attr = shard_arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));
                attr->type = SHARD_EXPR_SET;
                attr->loc = p->token.location;
                attr->set.recursive = recursive;
                shard_hashmap_init(p->ctx, &attr->set.attrs, 8);

                shard_hashmap_put(p->ctx, &set->set.attrs, key, attr);
                set = attr;
            }

            key = p->token.value.string;
            err = consume(p, SHARD_TOK_IDENT);
            if(err)
                break;
        }

        if(shard_hashmap_get(&set->set.attrs, key)) {
            err = errorf(p, "attribute `%s` already defined for this set", key);
        }

        if(consume(p, SHARD_TOK_ASSIGN) || parse_expr(p, value, PREC_LOWEST) || consume(p, SHARD_TOK_SEMICOLON)) {
            err = EINVAL;
            break;
        }
        
        shard_hashmap_put(p->ctx, &set->set.attrs, key, value);
    }

    int err3 = advance(p);
    return EITHER(err, err3);
}

static int parse_ternary(struct parser* p, struct shard_expr* expr) {
    expr->type = SHARD_EXPR_TERNARY;
    expr->loc = p->token.location;

    expr->ternary.cond = shard_arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));
    expr->ternary.if_branch = shard_arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));
    expr->ternary.else_branch = shard_arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));

    int err[] = {
        advance(p),
        parse_expr(p, expr->ternary.cond, PREC_LOWEST),
        consume(p, SHARD_TOK_THEN),
        parse_expr(p, expr->ternary.if_branch, PREC_LOWEST),
        consume(p, SHARD_TOK_ELSE),
        parse_expr(p, expr->ternary.else_branch, PREC_LOWEST),
    };

    return any_err(err, LEN(err));
}

static int parse_let_binding(struct parser* p, struct shard_binding* binding) {
    binding->loc = p->token.location;
    binding->ident = p->token.value.string;
    binding->value = shard_arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));

    int err[] = {
        consume(p, SHARD_TOK_IDENT),
        consume(p, SHARD_TOK_ASSIGN),
        parse_expr(p, binding->value, PREC_LOWEST),
        consume(p, SHARD_TOK_SEMICOLON)
    };

    return any_err(err, LEN(err));
}

static int parse_let(struct parser* p, struct shard_expr* expr) {
    expr->type = SHARD_EXPR_LET;
    expr->loc = p->token.location;
    expr->let.bindings = (struct shard_binding_list){0};
    expr->let.expr = shard_arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));

    int err = advance(p);
    while(p->token.type != SHARD_TOK_IN) {
        struct shard_binding binding;
        int err2 = parse_let_binding(p, &binding);
        dynarr_append(p->ctx, &expr->let.bindings, binding);

        if(err2)
            err = err2;
    }

    int errs[] = {
        err,
        advance(p),
        parse_expr(p, expr->let.expr, PREC_LOWEST)
    };

    return any_err(errs, LEN(errs));
}

static int parse_prefix_expr(struct parser* p, struct shard_expr* expr) {
    switch(p->token.type) {
        case SHARD_TOK_IDENT:
            return parse_ident(p, expr);
        case SHARD_TOK_INT:
            *expr = (struct shard_expr) {
                .type = SHARD_EXPR_INT,
                .loc = p->token.location,
                .floating = p->token.value.floating
            };
            return advance(p);
        case SHARD_TOK_FLOAT:
            *expr = (struct shard_expr) {
                .type = SHARD_EXPR_FLOAT,
                .loc = p->token.location,
                .floating = p->token.value.floating
            };
            return advance(p);
        case SHARD_TOK_NULL:
            *expr = (struct shard_expr) {
                .type = SHARD_EXPR_NULL,
                .loc = p->token.location,
            };
            return advance(p);
        case SHARD_TOK_TRUE:
            *expr = (struct shard_expr) {
                .type = SHARD_EXPR_TRUE,
                .loc = p->token.location,
            };
            return advance(p);
        case SHARD_TOK_FALSE:
            *expr = (struct shard_expr) {
                .type = SHARD_EXPR_FALSE,
                .loc = p->token.location,
            };
            return advance(p);
        case SHARD_TOK_STRING:
            return parse_string_lit(p, expr, SHARD_EXPR_STRING);
        case SHARD_TOK_PATH:
            return parse_string_lit(p, expr, SHARD_EXPR_PATH);
        case SHARD_TOK_WITH:
            return parse_with(p, expr);
        case SHARD_TOK_ASSERT:
            return parse_assert(p, expr);
        case SHARD_TOK_EXCLAMATIONMARK:
            return parse_unary_op(p, expr, SHARD_EXPR_NOT, PREC_NOT);
        case SHARD_TOK_SUB:
            return parse_unary_op(p, expr, SHARD_EXPR_NEGATE, PREC_NEGATION);
        case SHARD_TOK_LPAREN:
            return parse_compound(p, expr);
        case SHARD_TOK_LBRACKET:
            return parse_list(p, expr);
        case SHARD_TOK_LBRACE:
            return parse_set(p, expr, false);
        case SHARD_TOK_REC:
            return parse_set(p, expr, true);
        case SHARD_TOK_IF:
            return parse_ternary(p, expr);
        case SHARD_TOK_LET:
            return parse_let(p, expr);
        default: {
            if(p->token.type == SHARD_TOK_ERR)
                return EINVAL;

            static char buf[1024];
            shard_dump_token(buf, sizeof(buf), &p->token);
            int err = errorf(p, "unexpected token `%s`, expect expression", buf);
            advance(p);
            return err;
        }
    }

    return 0;
}

static int parse_binop(struct parser* p, struct shard_expr* expr, struct shard_expr* left, enum shard_expr_type type, enum precedence prec) {
    expr->type = type;
    expr->loc = p->token.location;
    expr->binop.lhs = shard_arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));
    expr->binop.rhs = shard_arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));
    
    *expr->binop.lhs = *left;

    int err[] = {
        advance(p),
        parse_expr(p, expr->binop.rhs, prec)
    };

    return any_err(err, LEN(err));
}

static int parse_call(struct parser* p, struct shard_expr* expr, struct shard_expr* left) {
    expr->type = SHARD_EXPR_CALL;
    expr->loc = p->token.location;
    expr->binop.lhs = shard_arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));
    expr->binop.rhs = shard_arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));

    *expr->binop.lhs = *left;

    return parse_expr(p, expr->binop.rhs, PREC_CALL);
}

static int parse_attribute_path(struct parser* p, struct shard_attr_path* path) {
    shard_attr_path_init(p->ctx, path);

    int err = 0, e;
    do {
        e = advance(p);
        if(e)
            err = e;
        dynarr_append(p->ctx, path, p->token.value.string);
        e = consume(p, SHARD_TOK_IDENT);
        if(e)
            err = e;
    } while(p->token.type == SHARD_TOK_PERIOD);

    return err;
}

static int parse_attribute_selection(struct parser* p, struct shard_expr* expr, struct shard_expr* set) {
    expr->type = SHARD_EXPR_ATTR_SEL;
    expr->loc = p->token.location;

    expr->attr_sel.set = shard_arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));
    expr->attr_sel.default_value = NULL;
    *expr->attr_sel.set = *set;

    int err[] = {
        parse_attribute_path(p, &expr->attr_sel.path),
        0,
        0
    };

    if(p->token.type == SHARD_TOK_OR) {
        err[1] = advance(p);
        expr->attr_sel.default_value = shard_arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));
        err[2] = parse_expr(p, expr->attr_sel.default_value, PREC_ATTRIBUTE_SELECTION);
    }
    
    return any_err(err, LEN(err));
}

static int parse_attribute_test(struct parser* p, struct shard_expr* expr, struct shard_expr* set) {
    expr->type = SHARD_EXPR_ATTR_TEST;
    expr->loc = p->token.location;

    expr->attr_test.set = shard_arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));
    *expr->attr_test.set = *set;

    return parse_attribute_path(p, &expr->attr_test.path);
}

static inline bool token_terminates_expr(enum shard_token_type type) {
    switch(type) {
        case SHARD_TOK_RPAREN:
        case SHARD_TOK_RBRACE:
        case SHARD_TOK_RBRACKET:
        case SHARD_TOK_SEMICOLON:
        case SHARD_TOK_COLON:
        case SHARD_TOK_EOF:
        case SHARD_TOK_ELSE:
        case SHARD_TOK_THEN:
        case SHARD_TOK_OR:
        case SHARD_TOK_ASSIGN:
        case SHARD_TOK_COMMA:
            return true;
        default:
            return false;
    }
}

static enum precedence get_precedence(enum shard_token_type type) {
    switch(type) {
        case SHARD_TOK_LOGIMPL:
            return PREC_LOGIMPL;
        case SHARD_TOK_LOGOR:
            return PREC_LOGOR;
        case SHARD_TOK_LOGAND:
            return PREC_LOGAND;
        case SHARD_TOK_EQ:
        case SHARD_TOK_NE:
            return PREC_EQUALITY;
        case SHARD_TOK_GE:
        case SHARD_TOK_GT:
        case SHARD_TOK_LE:
        case SHARD_TOK_LT:
            return PREC_COMPARISON;
        case SHARD_TOK_MERGE:
            return PREC_MERGE;
        case SHARD_TOK_ADD:
        case SHARD_TOK_SUB:
            return PREC_ADDITION;
        case SHARD_TOK_MUL:
        case SHARD_TOK_DIV:
            return PREC_MULTIPLICATION;
        case SHARD_TOK_CONCAT:
            return PREC_CONCATENATION;
        case SHARD_TOK_QUESTIONMARK:
            return PREC_ATTRIBUTE_TEST;
        case SHARD_TOK_PERIOD:
            return PREC_ATTRIBUTE_SELECTION;
        default:
            return token_terminates_expr(type) ? PREC_LOWEST : PREC_CALL;
    }
}

static int parse_infix_expr(struct parser* p, struct shard_expr* expr, struct shard_expr* left) {
    switch(p->token.type) {
        case SHARD_TOK_ADD:
            return parse_binop(p, expr, left, SHARD_EXPR_ADD, PREC_ADDITION);
        case SHARD_TOK_SUB:
            return parse_binop(p, expr, left, SHARD_EXPR_SUB, PREC_ADDITION);
        case SHARD_TOK_MUL:
            return parse_binop(p, expr, left, SHARD_EXPR_MUL, PREC_MULTIPLICATION);
        case SHARD_TOK_DIV:
            return parse_binop(p, expr, left, SHARD_EXPR_DIV, PREC_MULTIPLICATION);
        case SHARD_TOK_EQ:
            return parse_binop(p, expr, left, SHARD_EXPR_EQ, PREC_EQUALITY);
        case SHARD_TOK_NE:
            return parse_binop(p, expr, left, SHARD_EXPR_NE, PREC_EQUALITY);
        case SHARD_TOK_LOGIMPL:
            return parse_binop(p, expr, left, SHARD_EXPR_LOGIMPL, PREC_LOGIMPL);
        case SHARD_TOK_LOGOR:
            return parse_binop(p, expr, left, SHARD_EXPR_LOGOR, PREC_LOGOR);
        case SHARD_TOK_LOGAND:
            return parse_binop(p, expr, left, SHARD_EXPR_LOGAND, PREC_LOGAND);
        case SHARD_TOK_MERGE:
            return parse_binop(p, expr, left, SHARD_EXPR_MERGE, PREC_MERGE);
        case SHARD_TOK_CONCAT:
            return parse_binop(p, expr, left, SHARD_EXPR_CONCAT, PREC_CONCATENATION);
        case SHARD_TOK_PERIOD:
            return parse_attribute_selection(p, expr, left);
        case SHARD_TOK_QUESTIONMARK:
            return parse_attribute_test(p, expr, left);
        default:
            if(!token_terminates_expr(p->token.type))
                return parse_call(p, expr, left);
            else {
                if(p->token.type == SHARD_TOK_ERR)
                    return EINVAL;

                static char buf[1024];
                shard_dump_token(buf, sizeof(buf), &p->token);
                int err = errorf(p, "unexpected token `%s`, expect infix expression", buf);
                advance(p);
                return err;
            }
    }
}

static int parse_expr(struct parser* p, struct shard_expr* expr, enum precedence prec) {
    struct shard_expr tmp;
    int err = parse_prefix_expr(p, expr);

    while(prec < get_precedence(p->token.type)) {
        int err = parse_infix_expr(p, &tmp, expr);
        if(err)
            return err;
        *expr = tmp;
    }

    return err;
}

int shard_parse(struct shard_context* ctx, struct shard_source* src, struct shard_expr* expr) {
    struct parser p = {
        .ctx = ctx,
        .src = src,
        .token = {0}
    };

    int err[] = {
        advance(&p),
        parse_expr(&p, expr, PREC_LOWEST)
    };

    return any_err(err, LEN(err));
}

