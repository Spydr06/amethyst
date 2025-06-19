#ifndef _GEODE_LOG_H
#define _GEODE_LOG_H

#include "geode.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdnoreturn.h>

#define C_RED "\033[31m"
#define C_GREEN "\033[32m"
#define C_YELLOW "\033[93m"
#define C_BLACK "\033[90m"
#define C_BLUE "\033[34m"
#define C_PURPLE "\033[35m"

#define C_BLD "\033[1m"
#define C_RST "\033[0m"
#define C_NOBLD "\033[22m"

/*struct geode_logfile {
    char *path;
    FILE *file;
    
    struct geode_logfile *outer;
};

void geode_push_logfile(struct geode_context *context, const char *name);
char *geode_pop_logfile(struct geode_context *context);*/

__attribute__((format(printf, 2, 3)))
void geode_headingf(struct geode_context *context, const char *format, ...);
void geode_vheadingf(struct geode_context *context, const char *format, va_list ap);

__attribute__((format(printf, 2, 3)))
void geode_infof(struct geode_context *context, const char *format, ...);
void geode_vinfof(struct geode_context *context, const char *format, va_list ap);

__attribute__((format(printf, 2, 3)))
void geode_verbosef(struct geode_context *context, const char *format, ...);
void geode_vverbosef(struct geode_context *context, const char *format, va_list ap);

__attribute__((format(printf, 2, 3)))
void geode_errorf(struct geode_context *context, const char *format, ...);
void geode_verrorf(struct geode_context *context, const char *format, va_list ap);

__attribute__((format(printf, 2, 3)))
noreturn void geode_panic(struct geode_context *context, const char *format, ...);
noreturn void geode_vpanic(struct geode_context *context, const char *format, va_list ap);

#endif /* _GEODE_LOG_H */

