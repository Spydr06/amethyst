#include "log.h"

#include <stdio.h>
#include <stdnoreturn.h>

noreturn void geode_panic(struct geode_context *context, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    geode_vpanic(context, format, ap);
}

noreturn void geode_vpanic(struct geode_context *context, const char *format, va_list ap) {
    if(context->flags.err_no_color)
        fprintf(stderr, "%s: panic: ", context->progname);
    else
        fprintf(stderr, "%s: " C_BLD C_RED "panic:" C_RST " ", context->progname);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");

    va_end(ap);
    exit(EXIT_FAILURE);
}

void geode_headingf(struct geode_context *context, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    geode_vheadingf(context, format, ap);
    va_end(ap);
}

void geode_vheadingf(struct geode_context *context, const char *format, va_list ap) {
    if(!context->flags.out_no_color)
        printf(C_BLD C_BLUE);

    printf(" >> ");

    if(!context->flags.out_no_color)
        printf(C_PURPLE C_NOBLD);

    vprintf(format, ap);

    if(!context->flags.out_no_color)
        printf(C_RST);

    printf("\n");
}

void geode_infof(struct geode_context *context, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    geode_vinfof(context, format, ap);
    va_end(ap);
}

void geode_vinfof(struct geode_context *context, const char *format, va_list ap) {
    if(context->flags.err_no_color)
        fprintf(stderr, "%s: info: ", context->progname);
    else
        fprintf(stderr, "%s: " C_BLD C_PURPLE "info:" C_RST " ", context->progname);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
}

void geode_verbosef(struct geode_context *context, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    geode_vverbosef(context, format, ap);
    va_end(ap);
}

void geode_vverbosef(struct geode_context *context, const char *format, va_list ap) {
    if(context->flags.err_no_color)
        fprintf(stderr, "%s: info: ", context->progname);
    else
        fprintf(stderr, "%s: " C_BLD C_BLACK "info:" C_RST " ", context->progname);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
}


void geode_errorf(struct geode_context *context, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    geode_verrorf(context, format, ap);
    va_end(ap);
}

void geode_verrorf(struct geode_context *context, const char *format, va_list ap) {
    if(context->flags.err_no_color)
        fprintf(stderr, "%s: ""error: ", context->progname);
    else
        fprintf(stderr, "%s: " C_BLD C_RED "error:" C_RST " ", context->progname);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
}

