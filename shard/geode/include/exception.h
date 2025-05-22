#ifndef _GEODE_EXCEPTION_H
#define _GEODE_EXCEPTION_H

#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdnoreturn.h>

#include <libshard.h>
#include <git2/errors.h>

#ifdef __GNUC__
    #define MAX_STACK_LEVELS 16
#else
    #define MAX_STACK_LEVELS 0
#endif

struct geode_context;

enum geode_exception_type : uint32_t {
    GEODE_EX_SUBCOMMAND = 0b00000001,
    GEODE_EX_SHARD      = 0b00000010,
    GEODE_EX_IO         = 0b00000100,
    GEODE_EX_GIT        = 0b00001000,
    GEODE_EX_DERIV_DECL = 0b00010000,
    GEODE_EX_DERIV_DEP  = 0b00100000,
};

#define GEODE_PKG_EXCEPTION (GEODE_EX_PKG_DECL | GEODE_EX_PKG_DEP)
#define GEODE_ANY_EXCEPTION UINT32_MAX

#define GEODE_LOCALEXCEPT(ctx) __attribute__((cleanup(geode_pop_exception_handler, (ctx))))

struct geode_exception {
    enum geode_exception_type type;
    char *description;

    union {
        struct {
            int count;
            struct shard_error *errors;
        } shard;

        int ioerrno;

        struct {
            const git_error *error;
        } git;
    } payload;

    unsigned stacktrace_size;
    void *stacktrace[MAX_STACK_LEVELS];
};

typedef struct geode_exception exception_t;

struct geode_exception_handler {
    struct geode_exception_handler *outer;
    enum geode_exception_type catches; 
    volatile exception_t *thrown;
    jmp_buf jmp;
};

const char *exception_type_to_string(enum geode_exception_type type);

__attribute__((format(printf, 3, 4)))
noreturn void geode_throwf(struct geode_context *context, enum geode_exception_type type, const char *format, ...);
noreturn void geode_vthrowf(struct geode_context *context, enum geode_exception_type type, const char *format, va_list ap);

exception_t *geode_shard_ex(struct geode_context *context);
exception_t *geode_shard_ex2(struct geode_context *context, int num_errors, struct shard_error *errors);

exception_t *geode_io_ex(struct geode_context *context, int errno, const char *format, ...);

noreturn void geode_throw(struct geode_context *context, volatile exception_t *exception);

struct geode_exception_handler *__geode_catch(struct geode_context *context, enum geode_exception_type catches);

void geode_print_stacktrace(struct geode_context *context, void *stacktrace[], unsigned size);

#define geode_catch(context, catches) ({ \
        struct geode_exception_handler *handler = __geode_catch((context), (catches));  \
        int catch = setjmp(handler->jmp);                                               \
        volatile exception_t *e = NULL;                                                 \
        if(catch) {                                                                     \
            e = handler->thrown;                                                        \
            assert(e != NULL);                                                          \
            handler->thrown = NULL;                                                     \
        }                                                                               \
        e;                                                                              \
    })

void geode_push_exception_handler(struct geode_context *context, struct geode_exception_handler *handler);
void geode_pop_exception_handler(struct geode_context *context);

struct shard_value geode_exception_to_shard_value(struct shard_context *ctx, volatile exception_t *e);

#endif /* _GEODE_EXCEPTION_H */

