#ifndef _GEODE_H
#define _GEODE_H

#include <libshard.h>

void print_error(struct shard_error* error);

#ifdef unreachable
    #undef unreachable
#endif

#define unreachable() do {                                                                                              \
        fprintf(stderr, __FILE__ ":" __LINE__ ":%s `unreachable()` encountered. This should never happen.", __func__);  \
        exit(EXIT_FAILURE);                                                                                             \
    } while(0)

#endif /* _GEODE_H */

