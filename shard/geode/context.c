#if __STDC_VERSION__ >= 199901L
    #define _XOPEN_SOURCE 600
#else
    #define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */

#include <context.h>

#include <libshard.h>

#include "../shard.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <unistd.h>
#include <memory.h>

static int _getc(struct shard_source* src) {
    return fgetc(src->userp);
}

static int _ungetc(int c, struct shard_source* src) {
    return ungetc(c, src->userp);
}

static int _tell(struct shard_source* src) {
    return ftell(src->userp);
}

static int _close(struct shard_source* src) {
    return fclose(src->userp);
}

static int _open(const char* path, struct shard_source* dest, const char* restrict mode) {
    FILE* fd = fopen(path, mode);
    if(!fd)
        return errno;

    *dest = (struct shard_source) {
        .userp = (void*) fd,
        .origin = path,
        .getc = _getc,
        .ungetc = _ungetc,
        .tell = _tell,
        .close = _close,
        .line = 1
    };

    return 0;
}

static void print_basic_error(struct shard_error* error) {
    fprintf(stderr, 
        C_BLD C_RED "error: " C_RST C_BLD " %s:%u:" C_NOBLD " %s\n" C_RST,
        error->loc.src->origin, error->loc.line, EITHER(error->err, strerror(error->_errno))
    );
}

void geode_print_shard_error(struct shard_error* error) {
    if(error->loc.src->getc != _getc) {
        print_basic_error(error);
        return;
    }

    FILE* fd = error->loc.src->userp;
    assert(fd);

    size_t fd_pos = ftell(fd), line_start = error->loc.offset;

    while(line_start > 0) {
        fseek(fd, --line_start - 1, SEEK_SET);
        if(fgetc(fd) == '\n')
            break;
    }

    size_t line_end = error->loc.offset + error->loc.width - 1;
    fseek(fd, line_end, SEEK_SET);

    char c;
    while(((c = fgetc(fd)) != '\n') && c != EOF)
        line_end++;

    fseek(fd, line_start, SEEK_SET);

    char* line_str = calloc(line_end - line_start + 1, sizeof(char));
    fread(line_str, line_end - line_start, sizeof(char), fd);

    size_t column = error->loc.offset - line_start;

    fprintf(stderr, 
        C_BLD C_RED "error:" C_RST C_RST " %s\n " C_BLD C_BLUE "       at " C_PURPLE "%s:%u:%zu:\n" C_RST,
        EITHER(error->err, strerror(error->_errno)),
        error->loc.src->origin, error->loc.line, column
    );

    fprintf(stderr,
        C_BLD C_BLACK " %4d " C_NOBLD "| " C_RST,
        error->loc.line
    );

    fwrite(line_str, sizeof(char), column, stderr);
    fprintf(stderr, C_RED C_RST);
    fwrite(line_str + column, sizeof(char), error->loc.width, stderr);
    fprintf(stderr, C_RST "%s\n", line_str + column + error->loc.width);

    fprintf(stderr, C_BLACK "      |" C_RST " %*s" C_RED, (int) column, "");

    for(size_t i = 0; i < MAX(error->loc.width, 1); i++)
        fputc('^', stderr);
    fprintf(stderr, C_RST "\n");

    fseek(fd, fd_pos, SEEK_SET);
    free(line_str);
}

int geode_context_init(struct geode_context* ctx, const char* prog_name, geode_error_handler_t error_handler) {
    if(!ctx)
        return EINVAL;

    memset(ctx, 0, sizeof(struct geode_context));

    ctx->prog_name = prog_name;
    ctx->error_handler = error_handler;
    ctx->shard_ctx = (struct shard_context){
        .malloc = malloc,
        .realloc = realloc,
        .free = free,
        .realpath = realpath,
        .dirname = dirname,
        .access = access,
        .open = _open,
        .home_dir = nullptr
    };

    int err = shard_init(&ctx->shard_ctx);
    if(err)
        geode_throw(ctx, LIBSHARD_INIT, .err_no=err);

    ctx->shard_initialized = true;

    return 0;
}

void geode_context_free(struct geode_context* ctx) {
    if(ctx->shard_initialized) {
        shard_deinit(&ctx->shard_ctx);
        ctx->shard_initialized = false;
    }
}

_Noreturn void geode_throw_err(struct geode_context* ctx, struct geode_error err) {
    ctx->error_handler(ctx, err);
    unreachable();
}

