#include "git.h"

#include "exception.h"
#include "git2/global.h"

#include <memory.h>

#ifdef __GNUC__
#include <execinfo.h>
#endif

void git_lazy_init(struct geode_context *context) {
    if(context->flags.git_initialized)
        return;

    git_libgit2_init();
}

void git_shutdown(struct geode_context *context) {
    if(!context->flags.git_initialized)
        return;

    int err = git_libgit2_shutdown();
    if(err < 0) {
    }
}

exception_t *geode_git_ex(struct geode_context *context, const git_error *e, const char *format, ...) {
    va_list ap;
    va_start(ap);

    exception_t *ex = l_malloc(&context->l_global, sizeof(exception_t));
    memset(ex, 0, sizeof(exception_t));

    ex->type = GEODE_EX_GIT;
    ex->description = l_vsprintf(&context->l_global, format, ap);
    ex->payload.git.error = e;

#ifdef __GNUC__
    ex->stacktrace_size = backtrace(ex->stacktrace, MAX_STACK_LEVELS);
#endif

    return ex;
}
