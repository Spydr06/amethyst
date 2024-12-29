#if __STDC_VERSION__ >= 199901L
    #define _XOPEN_SOURCE 600
#else
    #define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */

#include <buildchain.h>

#include <assert.h>
#include <ctype.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static const char* tools[] = {
    "as",
    "addr2line",
    "ar",
    "cc",
    "cpp",
    "gcc",
    "ld",
    "nm",
    "ranlib",
    "readelf",
    "strings",
    "strip",
    NULL
};

static char* get_long_name(struct geode_context* ctx, const char* short_name) {
    const char* arch_str = geode_arch_to_string(ctx->cross_arch);

    size_t long_name_size = strlen(short_name) + strlen(arch_str) + 6;
    char* long_name = geode_malloc(ctx, long_name_size * sizeof(char));

    snprintf(long_name, long_name_size, "%s-elf-%s", arch_str, short_name);

    return long_name;
}

static void assert_tool_exists(struct geode_context* ctx, const char* tool) {
    char* path_env = getenv("PATH");
    if(!path_env)
        geode_throw(ctx, ELSE, ._else="Environment variable `PATH` is not set.");

    char* path = strdup(path_env);
    assert(path != NULL);
    
    char full_path[BUFSIZ];
    for(char* dir = strtok(path, ":"); dir; dir = strtok(NULL, ":")) {
        snprintf(full_path, __len(full_path), "%s/%s", dir, tool);
        if(access(full_path, X_OK) == 0)
            goto finish;
    }

    char* err = geode_malloc(ctx, 1024);
    snprintf(err, 1024, "Tool `%s` could not be found in your `PATH`.", tool);

    geode_throw(ctx, ELSE, ._else=err);
finish:
    free(path);
}

void buildchain_setup_cross_env(struct geode_context* ctx, struct geode_buildchain* cross_chain) {
    memset(cross_chain, 0, sizeof(struct geode_buildchain));

    shard_hashmap_init(&ctx->shard_ctx, &cross_chain->tools, __len(tools));
    for(const char** tool = tools; *tool; tool++) {
        char* long_name = get_long_name(ctx, *tool);
        assert_tool_exists(ctx, long_name);

        shard_hashmap_put(&ctx->shard_ctx, &cross_chain->tools, *tool, long_name);
    }
}

void buildchain_setenv(struct geode_context* ctx, struct geode_buildchain* buildchain) {
    (void) ctx;

    for(const char** tool = tools; *tool; tool++) {
        char* var = strdup(*tool);
        for(char* c = var; *c; c++)
            *c = toupper(*c);

        char* val = shard_hashmap_get(&buildchain->tools, *tool);
        assert(val != NULL);

        setenv(var, val, true);

        free(var);
    }
}

