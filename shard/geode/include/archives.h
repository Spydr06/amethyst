#ifndef _GEODE_ARCHIVE_H
#define _GEODE_ARCHIVE_H

#include <libshard.h>

struct shard_value builtin_archive_extractTar(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);

#endif /* _GEODE_ARCHIVE_H */

