#include "exception.h"

#include "geode.h"
#include "libshard.h"
#include "log.h"

#ifdef __GNUC__
#include <execinfo.h>
#endif

#include <assert.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

const char *exception_type_to_string(enum geode_exception_type type) {
    switch(type) {
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
    e->stacktrace_size = backtrace(e->stacktrace, MAX_STACK_LEVELS);
#endif

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

