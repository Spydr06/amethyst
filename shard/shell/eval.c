#include "eval.h"
#include "parse.h"

#include <assert.h>
#include <stdio.h>

int eval_next(struct shell_state* state) {
    assert(state->parser);

    struct shard_expr expr; 
    int err = shell_parse_next(state->parser, &expr);
    if(err)
        return err;

    // TODO: evaluate & save in state on success
    printf("expr: %d\n", expr.type);
    return 0;
}


int shell_eval(struct shell_state* state, struct shell_resource* resource) {
    struct shell_parser p;
    state->parser = &p;
    shell_parser_init(state->parser, resource);

    int err;
    while(!(err = eval_next(state)));

    shell_parser_free(state->parser);
    state->parser = NULL;

    return err == EOF ? 0 : err;
}

