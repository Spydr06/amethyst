#ifndef _GEODE_GIT_H
#define _GEODE_GIT_H

#include "geode.h"
#include "exception.h"

#include <git2.h>

void git_lazy_init(struct geode_context *context);

void git_shutdown(struct geode_context *context);

exception_t *geode_git_ex(struct geode_context *context, const git_error *e, const char *fmt, ...);

struct shard_value builtin_git_checkoutBranch(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);
struct shard_value builtin_git_pullRepo(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);
struct shard_value builtin_git_cloneRepo(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);

#endif /* _GEODE_GIT_H */

