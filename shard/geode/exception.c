#include "exception.h"

#include "geode.h"
#include "libshard.h"
#include "log.h"

#ifdef __GNUC__
#include <execinfo.h>
#endif

#include <archive.h>

#include <assert.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define INT_VAL(i) ((struct shard_value) { .type = SHARD_VAL_INT, .integer = (int64_t)(i) })
#define STRING_VAL(s, l) ((struct shard_value) { .type = SHARD_VAL_STRING, .string = (s), .strlen = (l) })
#define CSTRING_VAL(s) STRING_VAL(s, strlen((s)))

#define LIST_VAL(_head) ((struct shard_value) { .type = SHARD_VAL_LIST, .list.head = (_head) })
#define SET_VAL(_set) ((struct shard_value) { .type = SHARD_VAL_SET, .set = (_set) })

const char *exception_type_to_string(enum geode_exception_type type) {
    switch(type) {
    case GEODE_EX_SUBCOMMAND:
        return "subcommand";
    case GEODE_EX_IO:
        return "io";
    case GEODE_EX_GIT:
        return "git";
    case GEODE_EX_DERIV_DECL:
        return "derivation declaration";
    case GEODE_EX_DERIV_DEP:
        return "derivarion dependency";
    case GEODE_EX_NET:
        return "network";
    case GEODE_EX_ARCHIVE:
        return "archive";
    default:
        return "<unknown>";
    }
}

noreturn void geode_throwf(struct geode_context *context, enum geode_exception_type type, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    geode_vthrowf(context, type, format, ap);
}

noreturn void geode_vthrowf(struct geode_context *context, enum geode_exception_type type, const char *format, va_list ap) {
    exception_t *e = l_malloc(&context->l_global, sizeof(exception_t));
    memset(e, 0, sizeof(exception_t));

    e->type = type;
    e->description = l_vsprintf(&context->l_global, format, ap);

    va_end(ap);
    geode_throw(context, e);
}

exception_t *geode_shard_ex2(struct geode_context *context, int num_errors, struct shard_error *errors) {
    exception_t *e = l_malloc(&context->l_global, sizeof(exception_t));
    memset(e, 0, sizeof(exception_t));

    e->type = GEODE_EX_SHARD;
    e->description = l_sprintf(&context->l_global, "shard error:");
    e->payload.shard.errors = errors;
    e->payload.shard.count = num_errors;

    return e;
}

exception_t *geode_shard_ex(struct geode_context *context) {
    return geode_shard_ex2(context, shard_get_num_errors(&context->shard), shard_get_errors(&context->shard));
}

exception_t *geode_io_ex(struct geode_context *context, int errno, const char *format, ...) {
    va_list ap;
    va_start(ap, format);

    exception_t *e = l_malloc(&context->l_global, sizeof(exception_t));
    memset(e, 0, sizeof(exception_t));

    e->type = GEODE_EX_IO;
    e->description = l_vsprintf(&context->l_global, format, ap);
    e->payload.ioerrno = errno;

#ifdef __GNUC__
    void *stacktrace[MAX_STACK_LEVELS];
    e->stacktrace_size = backtrace(stacktrace, MAX_STACK_LEVELS);

    for(size_t i = 0; i < e->stacktrace_size; i++) {
        e->stacktrace[i] = l_strdup(&context->l_global, stacktrace[i]);
    }
#endif

    va_end(ap);
    return e;
}

exception_t *geode_archive_ex(struct geode_context *context, struct archive *a, const char *format, ...) {
    va_list ap;
    va_start(ap, format);

    exception_t *e = l_malloc(&context->l_global, sizeof(exception_t));
    memset(e, 0, sizeof(exception_t));

    e->type = GEODE_EX_ARCHIVE;
    e->description = l_sprintf(&context->l_global, "%s: %s", l_vsprintf(&context->l_global, format, ap), archive_error_string(a));

    va_end(ap);
    return e;
}

noreturn void geode_throw(struct geode_context *context, volatile exception_t *e) {
    struct geode_exception_handler *handler = context->e_handler;
    while(handler && !(handler->catches & e->type)) {
        handler = handler->outer;
    }

    if(!handler)
        geode_panic(context, "Unhandled exception `%s'.", exception_type_to_string(e->type));

    handler->thrown = e;
    longjmp(handler->jmp, (int) e->type);
}

struct geode_exception_handler *__geode_catch(struct geode_context *context, enum geode_exception_type catches) {
    struct geode_exception_handler *handler = malloc(sizeof(struct geode_exception_handler));
    handler->outer = NULL;
    handler->thrown = NULL;
    handler->catches = catches;

    geode_push_exception_handler(context, handler);

    return handler;
}

void geode_push_exception_handler(struct geode_context *context, struct geode_exception_handler *handler) {
    handler->outer = context->e_handler;
    context->e_handler = handler;
}

void geode_pop_exception_handler(struct geode_context *context) {
    struct geode_exception_handler *handler = context->e_handler;

    assert(handler && "No registered exception handler on pop");

    context->e_handler = context->e_handler->outer;
    free(handler);
}

void geode_print_stacktrace(struct geode_context *context, void *stacktrace[], unsigned size) {
#ifdef __GNUC__
    char **symbols = backtrace_symbols(stacktrace, size);
    if(!symbols)
        return;

    fprintf(stderr, "\n");
    geode_infof(context, "stack trace:");

    for(unsigned i = 0; i < size; i++) {
        fprintf(stderr, "% 4d: %s\n", i, symbols[i]);
    }

    free(symbols);
#endif
}

struct shard_value geode_exception_to_shard_value(struct shard_context *ctx, volatile exception_t *e) {
    struct shard_set *attrs = shard_set_init(ctx, 8);

    const char *ex_type = exception_type_to_string(e->type);
    shard_set_put(attrs, shard_get_ident(ctx, "exception"), shard_unlazy(ctx, CSTRING_VAL(ex_type))); 

    size_t desc_length = strlen(e->description);
    shard_set_put(attrs, shard_get_ident(ctx, "description"), shard_unlazy(ctx, STRING_VAL(shard_gc_strdup(ctx->gc, e->description, desc_length), desc_length)));

    struct shard_list *backtrace_head = NULL;
    for(unsigned i = e->stacktrace_size; i > 0; i--) {
        size_t cur_length = strlen(e->stacktrace[i-1]);

        struct shard_list *cur = shard_gc_malloc(ctx->gc, sizeof(struct shard_list));
        cur->next = backtrace_head;
        cur->value = shard_unlazy(ctx, STRING_VAL(shard_gc_strdup(ctx->gc, e->stacktrace[i-1], cur_length), cur_length));

        backtrace_head = cur;
    }

    shard_set_put(attrs, shard_get_ident(ctx, "backtrace"), shard_unlazy(ctx, LIST_VAL(backtrace_head)));

    switch(e->type) {
    case GEODE_EX_IO:
        shard_set_put(attrs,
            shard_get_ident(ctx, "errno"),
            shard_unlazy(ctx, CSTRING_VAL(strerror(e->payload.ioerrno)))
        );
        break;
    case GEODE_EX_GIT:
        shard_set_put(attrs,
            shard_get_ident(ctx, "class"),
            shard_unlazy(ctx, INT_VAL(e->payload.git.error->klass))
        );
        
        size_t message_length = strlen(e->payload.git.error->message);
        shard_set_put(attrs,
            shard_get_ident(ctx, "message"),
            shard_unlazy(ctx, STRING_VAL(shard_gc_strdup(ctx->gc, e->payload.git.error->message, message_length), message_length))
        );
        break;
    default:
    }

    return SET_VAL(attrs);
}

