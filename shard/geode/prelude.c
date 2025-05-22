#include "geode.h"
#include "libshard.h"

#include <libshard-util.h>

#include <errno.h>
#include <memory.h>
#include <stdlib.h>

static const char geode_prelude_source[] = "(import <modules>).loadModule \"geode\"";

int geode_prelude_attr(struct geode_context *context, const char *attr_name, struct shard_value *value) {
    if(!context->prelude)
        geode_prelude(context);

    assert(context->prelude);

    shard_ident_t attr = shard_get_ident(&context->shard, attr_name); 
    struct shard_lazy_value *lazy_value = NULL;

    int err = shard_set_get(context->prelude, attr, &lazy_value);
    if(err)
        return err;

    if((err = shard_eval_lazy(&context->shard, lazy_value)))
        geode_throw(context, geode_shard_ex(context));

    assert(lazy_value->evaluated);
    *value = lazy_value->eval;

    return 0;
}

void geode_prelude(struct geode_context *context) {
    shard_include_dir(&context->shard, context->module_path.string);

    struct shard_source prelude_source;
    int err = shard_string_source(&context->shard, &prelude_source, "<geode prelude>", geode_prelude_source, sizeof(geode_prelude_source), 0);
    if(err)    
        geode_throw(context, geode_shard_ex(context));

    struct shard_open_source *open_source = malloc(sizeof(struct shard_open_source));
    memset(open_source, 0, sizeof(struct shard_open_source));
    open_source->source = prelude_source;
    open_source->opened = true;

    open_source->auto_close = true;
    open_source->auto_free = true;
    shard_register_open(&context->shard, "prelude.shard", false, open_source);

    if((err = shard_eval(&context->shard, open_source)))
        geode_throw(context, geode_shard_ex(context));

    struct shard_value prelude_result = open_source->result;
    if(prelude_result.type != SHARD_VAL_SET)
        geode_throw(context, geode_io_ex(context, EINVAL, 
                "expected `prelude.shard` to yield value of type `set`, git `%s` instead",
                shard_value_type_to_string(&context->shard, prelude_result.type)
        ));

    shard_gc_make_static(context->shard.gc, prelude_result.set);
    
    context->prelude = prelude_result.set;
}

void geode_call_with_prelude(struct geode_context *context, struct shard_value *result, struct shard_value function) {
    if(!context->prelude)
        geode_prelude(context);

    assert(context->prelude);
    
    if(shard_call(&context->shard, function, &(struct shard_value){.type = SHARD_VAL_SET, .set = context->prelude}, result))
        geode_throw(context, geode_shard_ex(context));
}

