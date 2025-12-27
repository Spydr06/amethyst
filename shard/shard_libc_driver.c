#define _XOPEN_SOURCE 700

#include "shard_libc_driver.h"

#include <assert.h>
#include <errno.h>
#include <libgen.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define EITHER(a, b) ((a) ? (a) : (b))

#define C_RED "\033[31m"
#define C_YELLOW "\033[93m"
#define C_BLACK "\033[90m"
#define C_BLUE "\033[34m"
#define C_PURPLE "\033[35m"

#define C_BLD "\033[1m"
#define C_RST "\033[0m"
#define C_NOBLD "\033[22m"

#define C(b, c) ((b) ? (c) : "")

int shard_close_callback(struct shard_source* self) {
    return fclose((FILE*) self->userp);
}

int shard_read_all_callback(struct shard_source* self, struct shard_string* dest) {
    FILE* fp = (FILE*) self->userp;

    ssize_t previous_location = ftell(fp);
    if(previous_location < 0)
        return errno;

    if(fseek(fp, 0, SEEK_END))
        return errno;

    ssize_t file_size = ftell(fp);
    if(file_size < 0)
        return errno;

    if(fseek(fp, previous_location, SEEK_SET))
        return errno;
    
    dest->capacity = file_size + 1;
    dest->items = malloc((file_size + 1) * sizeof(char));
    dest->count = fread(dest->items, sizeof(char), file_size, fp);
    dest->items[dest->count++] = '\0';

    return 0;
}

void shard_buffer_dtor_callback(struct shard_string* buffer) {
    if(buffer->items)
        free(buffer->items);
    memset(buffer, 0, sizeof(struct shard_string));
}

int shard_open_callback(const char* path, struct shard_source* dest, const char* restrict mode) {
    FILE* fp = fopen(path, mode);
    if(!fp)
        return errno;

    *dest = (struct shard_source) {
        .userp = fp,
        .origin = path,
        .close = shard_close_callback,
        .read_all = shard_read_all_callback,
        .buffer_dtor = shard_buffer_dtor_callback,
        .line_offset = 1,
    };

    return 0;
}

int shard_context_default(struct shard_context* ctx) {
    *ctx = (struct shard_context) {
        .malloc = malloc,
        .realloc = realloc,
        .free = free,
        .realpath = realpath,
        .dirname = dirname,
        .access = access,
        .R_ok = R_OK,
        .W_ok = W_OK,
        .X_ok = X_OK,
        .open = shard_open_callback,
        .home_dir = getenv("HOME")
    };

    return 0;
}

static void print_basic_error(FILE* stream, struct shard_error* error, bool color) {
    fprintf(stream, "%serror:%s %s\n", 
            C(color, C_BLD C_RED), C(color, C_RST),
            EITHER(error->err, strerror(error->_errno)));

    fprintf(stream, "%s       at%s %s:%u:%u:%s\n",
            C(color, C_BLD C_BLUE), C(color, C_PURPLE),
            error->loc.src ? error->loc.src->origin : "<unknown>", error->loc.line, error->loc.column,
            C(color, C_RST));
}

static void print_file_error(FILE* stream, struct shard_error* error, bool color) {
    FILE* fd = error->loc.src->userp;
    assert(fd);

    size_t fd_pos = ftell(fd);
    size_t line_start = error->loc.offset - error->loc.column + 1;
    size_t line_end = error->loc.offset + error->loc.width - 1;
    fseek(fd, line_end, SEEK_SET);

    char c;
    while(((c = fgetc(fd)) != '\n') && c != EOF)
        line_end++;

    fseek(fd, line_start, SEEK_SET);

    char* line_str = calloc(line_end - line_start + 1, sizeof(char));
    (void) !! fread(line_str, sizeof(char), line_end - line_start, fd);

    print_basic_error(stream, error, color);

    fprintf(stderr, "%s % 4d |%s ", C(color, C_BLACK), (int) error->loc.line, C(color, C_RST));

    fwrite(line_str, sizeof(char), error->loc.column, stderr);
    fprintf(stderr, "%s", C(color, C_RED C_RST));
    fwrite(line_str + error->loc.column, sizeof(char), error->loc.width, stderr);
    fprintf(stderr, "%s%s\n", C(color, C_RST), line_str + error->loc.column + error->loc.width);

    fprintf(stderr, "%s      |%s %*s%s", C(color, C_BLACK), C(color, C_RST), (int) error->loc.column, "", C(color, C_RED));

    for(size_t i = 0; i < MAX(error->loc.width, 1); i++)
        fputc('^', stderr);
    fprintf(stderr, "%s\n", C(color, C_RST));

    fseek(fd, fd_pos, SEEK_SET);
    free(line_str);
}

void print_shard_error(FILE* stream, struct shard_error* error) {
    bool colorize = stream == stdout || stream == stderr;

    if(!error->loc.src ||  error->loc.src->close != shard_close_callback)
        print_basic_error(stream, error, colorize);
    else
        print_file_error(stream, error, colorize);
}

void shard_emit_errors(struct shard_context *ctx) {
    struct shard_error* errors = shard_get_errors(ctx);
    for(size_t i = 0; i < shard_get_num_errors(ctx); i++)
        print_shard_error(stderr, &errors[i]);

    shard_remove_errors(ctx);
}
