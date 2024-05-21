#define _LIBSHARD_INTERNAL
#include <libshard.h>

#include <assert.h>

int shard_init(struct shard_context* context) {
    assert(context->malloc && context->realloc && context->free);
    context->idents = arena_init(context);
    context->errors = (struct shard_errors){0};
    context->string_literals = (struct shard_string_list){0};
    return 0;
}

void shard_deinit(struct shard_context* context) {
    arena_free(context, context->idents);
    dynarr_free(context, &context->errors);

    for(size_t i = 0; i < context->string_literals.count; i++)
        context->free(context->string_literals.items[i]);
    dynarr_free(context, &context->string_literals);
}

int shard_run(struct shard_context* context, struct shard_source* src) {
    struct shard_token token;

    do {
        int err = shard_lex(context, src, &token);
        if(err) {
            struct shard_error e = {
                .loc = token.location,
                .err = token.value.string,
                ._errno = err
            };
            dynarr_append(context, &context->errors, e);
            break;
        }
        
        printf("token %d\n", token.type);
    } while(token.type != SHARD_TOK_EOF);

    return context->errors.count;
}

struct shard_error* shard_get_errors(struct shard_context* context) {
    return context->errors.items;
}

