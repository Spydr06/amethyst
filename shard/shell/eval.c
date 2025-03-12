#include "eval.h"
#include "parse.h"
#include "stmt.h"

#include <assert.h>
#include <stdio.h>

int eval_next(struct shell_state* state) {
    assert(state->parser);

    struct shell_stmt stmt; 
    int err = shell_parse_next(state->parser, &stmt);
    if(err)
        return err;

    // TODO: evaluate & save in state on success
    printf("stmt");
    return 0;
}


int shell_eval(struct shell_state* state, struct shell_resource* resource) {
    struct shell_parser p;
    state->parser = &p;
    shell_parser_init(state->parser, resource, state->shard_context.gc);

    int err;
    while(!(err = eval_next(state)));

    shell_parser_free(state->parser);
    state->parser = NULL;

    return err == EOF ? 0 : err;
}

