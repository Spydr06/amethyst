#ifndef _GEODE_CONTEXT_H
#define _GEODE_CONTEXT_H

#include "config.h"
#include <libshard.h>

#ifndef __len
    #define __len(arr) (sizeof((arr)) / sizeof(*(arr)))
#endif

#define _(str) str

struct geode_error;
struct geode_context;

typedef __attribute__((noreturn)) void (*geode_error_handler_t)(struct geode_context*, struct geode_error);

enum geode_error_type {
    GEODE_ERR_LIBSHARD_INIT = 1,
    GEODE_ERR_SHARD,
    GEODE_ERR_CONFIGURATION,
    GEODE_ERR_UNRECOGNIZED_ACTION,
    GEODE_ERR_UNRECOGNIZED_ARGUMENT,
    GEODE_ERR_NO_ACTION,
    GEODE_ERR_MISSING_PARAMETER,
    GEODE_ERR_FILE_IO,
    GEODE_ERR_MKDIR,
    GEODE_ERR_ELSE
};

enum geode_architecture {
    GEODE_ARCH_UNKNOWN = 0,
    GEODE_ARCH_X86_64 = 1,
    // Expand when more architectures are supported by the kernel
    __GEODE_ARCH_NUM
};

struct geode_error {
    enum geode_error_type type;
    union {
        int err_no;
        const char* action;
        const char* argument;
        struct { const char* path; int err_no; } file;
        struct { struct shard_error* errs; int num; } shard;
        struct { struct shard_value* val; const char* msg; } config;
        const char* _else;
    } payload;
};

struct heap_bucket {
    struct heap_bucket *prev, *next;
    uint8_t data[];
};

struct geode_context {
    const char* prog_name;
    const char* current_action;

    int nproc;

    struct {
        bool verbose   : 1;
        int __unused__ : 7;
    } flags;

    struct shard_context shard_ctx;
    bool shard_initialized;

    enum geode_architecture arch;
    enum geode_architecture cross_arch;

    char* main_config_path;
    char* store_path;
    const char* prefix;

    struct shard_value* configuration;

    geode_error_handler_t error_handler; 

    struct heap_bucket* heap;
};

int geode_context_init(struct geode_context* ctx, const char* prog_name, geode_error_handler_t error_handler);
void geode_context_free(struct geode_context* ctx);

void* geode_malloc(struct geode_context* ctx, size_t size);
void* geode_calloc(struct geode_context* ctx, size_t nmemb, size_t size);
void* geode_realloc(struct geode_context* ctx, void* ptr, size_t size);
void geode_free(struct geode_context* ctx, void* ptr);

char* geode_strdup(struct geode_context* ctx, const char* str);

void geode_context_set_prefix(struct geode_context* ctx, const char* prefix);
void geode_context_set_config_file(struct geode_context* ctx, char* config_file);
void geode_context_set_store_path(struct geode_context* ctx, char* store_path);

int geode_context_set_nproc(struct geode_context* ctx, const char* nproc_arg);

struct shard_value geode_call_file(struct geode_context* ctx, const char* script_path);

int geode_open_shard_file(const char* path, struct shard_source* dest, const char* restrict mode);

void geode_print_shard_error(struct shard_error* err);

const char* geode_arch_to_string(enum geode_architecture arch);
enum geode_architecture geode_string_to_arch(const char* str);

#define geode_throw(_ctx, _type, _payload) (geode_throw_err((_ctx), (struct geode_error){.type = GEODE_ERR_##_type, .payload={_payload}}))

_Noreturn void geode_throw_err(struct geode_context* ctx, struct geode_error err);

#endif /* _GEODE_CONTEXT_H */

