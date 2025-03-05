#include "resource.h"
#include "shell.h"

#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

struct repl { 
    char** history;
    size_t history_size;
    size_t history_next_entry;

    struct shard_string current;
    size_t read_offset;
};

static int repl_getc(struct shell_resource* resource) {
    struct repl* repl = resource->userp;

    char c = repl->current.items[repl->read_offset];
    if(c) {
        repl->read_offset++;
        return c;
    }
    
    return EOF;
}

static int repl_ungetc(struct shell_resource* resource, int c) {
    struct repl* repl = resource->userp;

    if(repl->read_offset == 0) 
        return EOF;

    repl->read_offset--;
    if(repl->current.items[repl->read_offset] != c)
        return EOF;

    return c;
}

void repl_resource(struct repl* repl, struct shell_resource* resource) {
    memset(resource, 0, sizeof(struct shell_resource));

    resource->userp = repl;
    resource->getc = repl_getc;
    resource->ungetc = repl_ungetc;
}

void repl_init(struct repl *repl) {
    memset(repl, 0, sizeof(struct repl));

    repl->history = calloc(shell.history_size, sizeof(const char*));
    repl->history_size = shell.history_size;
}

void repl_free(struct repl* repl) {
    for(size_t i = 0; i < repl->history_size; i++) {
        if(repl->history[i])
            free(repl->history[i]);
    }

    free(repl->history);
}

bool repl_should_close(struct repl* repl) {
    return false;
}

int repl_readline(struct repl* repl, FILE* stream) {
    repl->current.count = 0; 

    for(;;) {
        char c = fgetc(stream);
    }
}

void repl_push_history(struct repl* repl, const char* str) {
    if(repl->history[repl->history_next_entry])
        free(repl->history[repl->history_next_entry]);

    repl->history[repl->history_next_entry] = strdup(str);
    repl->history_next_entry = (repl->history_next_entry + 1) % repl->history_size;
}

int shell_repl(void) {
    struct repl repl;
    struct shell_resource resource;
    struct shell_state state;

    repl_init(&repl);
    repl_resource(&repl, &resource);
    shell_state_init(&state, &resource);

    int err = 0;

    while(!repl_should_close(&repl)) {
        printf("$ ");
        fflush(stdout);

        err = repl_readline(&repl, stdin);
        if(err && shell.exit_on_error)
            break;

        printf("> %s\n", repl.current.items);

        repl_push_history(&repl, repl.current.items);
    }
    
    shell_state_free(&state);
    resource_free(&resource);
    repl_free(&repl);

    return err;
}


