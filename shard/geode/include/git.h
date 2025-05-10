#ifndef _GEODE_GIT_H
#define _GEODE_GIT_H

#include "geode.h"
#include "exception.h"

#include <git2.h>

void git_lazy_init(struct geode_context *context);

void git_shutdown(struct geode_context *context);

exception_t *geode_git_ex(struct geode_context *context, const git_error *e, const char *fmt, ...);

#endif /* _GEODE_GIT_H */

