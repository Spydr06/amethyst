#include "shell.h"

#define _XOPEN_SOURCE 600

#include <unistd.h>
#include <libgen.h>
#include <errno.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

#include "resource.h"

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

    *dest = (struct shard_source){
        .userp = fd,
        .origin = path,
        .getc = _getc,
        .ungetc = _ungetc,
        .tell = _tell,
        .close = _close,
        .line = 1
    };

    return 0;
}

void shell_state_init(struct shell_state* state, struct shell_resource* resource) {
    memset(state, 0, sizeof(struct shell_state));

    state->resource = resource;
    state->shard_context = (struct shard_context){
        .malloc = malloc,
        .free = free,
        .realloc = realloc,
        .realpath = realpath,
        .dirname = dirname,
        .access = access,
        .R_ok = R_OK,
        .W_ok = W_OK,
        .X_ok = X_OK,
        .open = _open,
        .home_dir = getenv("HOME"),
    };

    int err = shard_init(&state->shard_context);
    if(err) {
        errorf("libshard: %s", strerror(err));
        exit(EXIT_FAILURE);
    }

    shard_set_current_system(&state->shard_context, SHELL_HOST_SYSTEM);
}

void shell_state_free(struct shell_state* state) {
    shard_deinit(&state->shard_context);
}

