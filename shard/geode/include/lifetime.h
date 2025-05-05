#ifndef _GEODE_LIFETIME_H
#define _GEODE_LIFETIME_H

//
// Forked and adapted from https://github.com/tsoding/arena
//
// Copyright 2022 Alexey Kutepov <reximkut@gmail.com>
// Under the MIT License
//

#include <stdarg.h>
#include <stddef.h>

struct lifetime_region;

struct lifetime {
    struct lifetime_region *begin, *end;
};

typedef struct lifetime lifetime_t;

void *l_malloc(lifetime_t *l, size_t size);
void *l_calloc(lifetime_t *l, size_t nmemb, size_t size);
void *l_realloc(lifetime_t *l, void *old_ptr, size_t new_size);

char *l_strdup(lifetime_t *l, const char *str);
void *l_memdup(lifetime_t *l, const void *data, size_t size);

char *l_strcat(lifetime_t *l, const char *a, const char *b);

char *l_sprintf(lifetime_t *l, const char *format, ...);
char *l_vsprintf(lifetime_t *l, const char *format, va_list ap);

void l_reset(lifetime_t *l);
void l_free(lifetime_t *l);
void l_trim(lifetime_t *l);

#endif /* _GEODE_LIFETIME_H */

