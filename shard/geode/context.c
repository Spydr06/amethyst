#if __STDC_VERSION__ >= 199901L
    #define _XOPEN_SOURCE 600
#else
    #define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */

#include <geode.h>
#include <context.h>
#include <config.h>
#include <builtins.h>

#include <libshard.h>

#include <assert.h>
#include <errno.h>
#include <libgen.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __x86_64__
#define HOST_ARCH GEODE_ARCH_X86_64
#else
#define HOST_ARCH GEODE_ARCH_UNKNOWN
#endif

static const char* arch_strings[] = {
    "unknown",
    "x86_64"
};

static_assert(__len(arch_strings) == __GEODE_ARCH_NUM);

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

int geode_open_shard_file(const char* path, struct shard_source* dest, const char* restrict mode) {
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
    (void) fread(line_str, line_end - line_start, sizeof(char), fd);

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

    ctx->main_config_path = GEODE_DEFAULT_PREFIX GEODE_DEFAULT_CONFIG_FILE;
    ctx->store_path       = GEODE_DEFAULT_PREFIX GEODE_DEFAULT_STORE_PATH;
    ctx->prefix           = GEODE_DEFAULT_PREFIX;
    ctx->arch = HOST_ARCH;
    ctx->cross_arch = HOST_ARCH;

    ctx->nproc = 1;

    ctx->shard_ctx = (struct shard_context){
        .malloc = malloc,
        .realloc = realloc,
        .free = free,
        .realpath = realpath,
        .dirname = dirname,
        .access = access,
        .R_ok = R_OK,
        .W_ok = W_OK,
        .X_ok = X_OK,
        .open = geode_open_shard_file,
        .home_dir = nullptr,
        .userp = ctx
    };

    int err = shard_init(&ctx->shard_ctx);
    if(err)
        geode_throw(ctx, LIBSHARD_INIT, .err_no=err);

    shard_include_dir(&ctx->shard_ctx, ctx->store_path);

    ctx->shard_initialized = true;

    return 0;
}

void geode_context_free(struct geode_context* ctx) {
    if(ctx->shard_initialized) {
        shard_deinit(&ctx->shard_ctx);
        ctx->shard_initialized = false;
    }

    // free all heap buckets
    struct heap_bucket* bucket = ctx->heap;

    while(bucket) {
        struct heap_bucket* next = bucket->next;
        free(bucket);
        bucket = next;
    }
}

char* geode_strdup(struct geode_context* ctx, const char* str) {
    size_t size = strlen(str) + 1;
    char* new = geode_malloc(ctx, size);
    memcpy(new, str, size);
    return new;
}

void* geode_malloc(struct geode_context* ctx, size_t size) {
    struct heap_bucket* bucket = malloc(sizeof(struct heap_bucket) + size);
    if(!bucket)
        return NULL;
    
    bucket->next = NULL;
    bucket->prev = NULL;

    if(ctx->heap) {
        struct heap_bucket* prev = ctx->heap;
        while(prev->next) 
            prev = prev->next;
        
        prev->next = bucket;
        bucket->prev = prev;
    }
    else {
        ctx->heap = bucket;
    }

    return bucket + 1;
}

void* geode_calloc(struct geode_context* ctx, size_t nmemb, size_t size) {
    void* ptr = geode_malloc(ctx, nmemb * size);
    memset(ptr, 0, nmemb * size);
    return ptr;
}

void* geode_realloc(struct geode_context* ctx, void* ptr, size_t size) {
    if(!ptr)
        return geode_malloc(ctx, size);

    struct heap_bucket* bucket = ((struct heap_bucket*) ptr) - 1;
    bucket = realloc(bucket, sizeof(struct heap_bucket) + size);

    if(bucket->prev)
        bucket->prev->next = bucket;
    else
        ctx->heap = bucket;

    if(bucket->next)
        bucket->next->prev = bucket;

    return bucket + 1;
}

void geode_free(struct geode_context* ctx, void* ptr) {
    if(!ptr)
        return;

    struct heap_bucket* bucket = ((struct heap_bucket*) ptr) - 1;

    if(bucket->prev)
        bucket->prev->next = bucket->next;
    if(bucket->next)
        bucket->next->prev = bucket->prev;

    if(bucket == ctx->heap)
        ctx->heap = NULL;

    free(bucket);
}

void geode_context_set_prefix(struct geode_context* ctx, const char* prefix) {
    ctx->prefix = prefix;

    if(strcmp(ctx->main_config_path, GEODE_DEFAULT_PREFIX GEODE_DEFAULT_CONFIG_FILE) == 0) {
        size_t len = strlen(ctx->prefix) + strlen(GEODE_DEFAULT_CONFIG_FILE) + 2;
        ctx->main_config_path = geode_malloc(ctx, len);
        snprintf(ctx->main_config_path, len, "%s/%s", ctx->prefix, GEODE_DEFAULT_CONFIG_FILE);
    }

    if(strcmp(ctx->store_path, GEODE_DEFAULT_PREFIX GEODE_DEFAULT_STORE_PATH) == 0) {
        size_t len = strlen(ctx->prefix) + strlen(GEODE_DEFAULT_STORE_PATH) + 2;
        ctx->main_config_path = geode_malloc(ctx, len);
        snprintf(ctx->main_config_path, len, "%s/%s", ctx->prefix, GEODE_DEFAULT_STORE_PATH);
    }
}

void geode_context_set_config_file(struct geode_context* ctx, char* config_file) {
    ctx->main_config_path = config_file;
}

void geode_context_set_store_path(struct geode_context* ctx, char* store_path) {
    for(size_t i = 0; i < ctx->shard_ctx.include_dirs.count; i++) {
        const char* include_path = ctx->shard_ctx.include_dirs.items[i];
        if(strcmp(include_path, ctx->store_path) == 0) {
            ctx->shard_ctx.include_dirs.items[i] = store_path;
            ctx->store_path = store_path;
            return;
        }
    }

    shard_include_dir(&ctx->shard_ctx, store_path);
    ctx->store_path = store_path;
}

int geode_context_set_nproc(struct geode_context* ctx, const char* nproc_arg) {
    errno = 0;
    ctx->nproc = (int) strtol(nproc_arg, NULL, 10);
    return errno;
}

_Noreturn void geode_throw_err(struct geode_context* ctx, struct geode_error err) {
    ctx->error_handler(ctx, err);
    unreachable();
}

struct shard_value geode_call_file(struct geode_context* ctx, const char* script_path) {
    struct shard_open_source* source = shard_open(&ctx->shard_ctx, script_path);
    if(!source)
        geode_throw(ctx, FILE_IO, .file=TUPLE(.err_no=errno,.path=script_path));
    
    int err = shard_eval(&ctx->shard_ctx, source);
    if(err)
        geode_throw(ctx, SHARD, .shard=TUPLE(.num=err,.errs=shard_get_errors(&ctx->shard_ctx)));

    infof(ctx, "Loaded script `%s`.\n", script_path);

    struct shard_value result = {0};
    err = shard_call(&ctx->shard_ctx, source->result, ctx->configuration, &result);
    if(err)
        geode_throw(ctx, SHARD, .shard=TUPLE(.num=err, .errs=shard_get_errors(&ctx->shard_ctx)));

    return result;
}

const char* geode_arch_to_string(enum geode_architecture arch) {
    return arch >= 0 && arch < __GEODE_ARCH_NUM 
        ? EITHER(arch_strings[arch], arch_strings[GEODE_ARCH_UNKNOWN])
        : arch_strings[GEODE_ARCH_UNKNOWN];
}

enum geode_architecture geode_string_to_arch(const char* str) {
    for(enum geode_architecture a = 0; a < __GEODE_ARCH_NUM; a++) {
        if(strcmp(str, arch_strings[a]) == 0)
            return a;
    }

    return GEODE_ARCH_UNKNOWN;
}

