#define _LIBSHARD_INTERNAL
#include <libshard.h>

#include <ctype.h>
#include <libgen.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>

struct shard_builtin_const {
    const char* full_name;
    struct shard_value value;
};

SHARD_DECL struct shard_value shard_builtin_eval_arg(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args, unsigned i) {
    if(i >= builtin->arity)
        shard_eval_throw(e, e->error_scope->loc, "%s: declares `%u` arguments but requested index `%u`", builtin->full_ident, builtin->arity, i);

    struct shard_value value = shard_eval_lazy2(e, args[i]);
    if(!(value.type & builtin->arg_types[i]))
        shard_eval_throw(e, 
                e->error_scope->loc,
                "%s: expect argument %u to be of type `%s`, got `%s`",
                builtin->full_ident,
                i,
                shard_value_type_to_string(e->ctx, builtin->arg_types[i]),
                shard_value_type_to_string(e->ctx, value.type)
        );

    return value;
}

static struct shard_value builtin_add(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    return shard_eval_addition(e,
        shard_builtin_eval_arg(e, builtin, args, 0),
        shard_builtin_eval_arg(e, builtin, args, 1),
        &e->error_scope->loc        
    );
}

static struct shard_value builtin_sub(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    return shard_eval_subtraction(e,
        shard_builtin_eval_arg(e, builtin, args, 0),
        shard_builtin_eval_arg(e, builtin, args, 1),
        &e->error_scope->loc        
    );
}

static struct shard_value builtin_mul(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    return shard_eval_multiplication(e,
        shard_builtin_eval_arg(e, builtin, args, 0),
        shard_builtin_eval_arg(e, builtin, args, 1),
        &e->error_scope->loc        
    );
}

static struct shard_value builtin_div(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    return shard_eval_division(e,
        shard_builtin_eval_arg(e, builtin, args, 0),
        shard_builtin_eval_arg(e, builtin, args, 1),
        &e->error_scope->loc        
    );
}

static struct shard_value builtin_not(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    return BOOL_VAL(!shard_builtin_eval_arg(e, builtin, args, 0).boolean);
}


static struct shard_value builtin_elem(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value want = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value list = shard_builtin_eval_arg(e, builtin, args, 1);

    struct shard_list* head = list.list.head;
    while(head) {
        struct shard_value elem = shard_eval_lazy2(e, head->value);
        if(shard_values_equal(e, &want, &elem))
            return BOOL_VAL(true);

        head = head->next;
    }

    return BOOL_VAL(false);
}

static struct shard_value builtin_elemAt(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value list = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value index = shard_builtin_eval_arg(e, builtin, args, 1);

    struct shard_list* head = list.list.head;

    if(index.integer < 0)
        return NULL_VAL();

    int64_t i = 0;

    while(head) {
        if(i == index.integer) 
            return shard_eval_lazy2(e, head->value);

        i++;
        head = head->next;
    }

    return NULL_VAL();
}

static struct shard_value builtin_errnoString(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args)  {
    struct shard_value err = shard_builtin_eval_arg(e, builtin, args, 0);
    return CSTRING_VAL(strerror((int) err.integer));
}

static struct shard_value builtin_evaluated(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    (void) e;
    (void) builtin;
    return BOOL_VAL((*args)->evaluated);
}

static struct shard_value builtin_floor(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);
    return INT_VAL((int64_t) arg.floating);
}

static struct shard_value builtin_ceil(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);
    return INT_VAL((int64_t) (arg.floating + 1.0));
}

static struct shard_value builtin_char(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    static char ascii_table[0xff * 2];
    static bool ascii_table_init = false;

    if(!ascii_table_init) {
        for(size_t i = 0; i < 0xff; i++) {
            ascii_table[i * 2] = (char) i;
            ascii_table[i * 2 + 1] = '\0';
        }

        ascii_table_init = true;
    }

    struct shard_value ascii_val = shard_builtin_eval_arg(e, builtin, args, 0);

    if(ascii_val.integer >= 0 && ascii_val.integer < 0xff)
        return STRING_VAL(&ascii_table[ascii_val.integer], 1);
    return NULL_VAL();
}

static struct shard_value builtin_charAt(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value index = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value string = shard_builtin_eval_arg(e, builtin, args, 1);

    if(index.integer >= 0 && string.strlen > (size_t) index.integer)
        return INT_VAL(string.string[index.integer]);
    return NULL_VAL();
}

static struct shard_value builtin_all(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value pred = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value list = shard_builtin_eval_arg(e, builtin, args, 1);

    bool all_result = true;
    struct shard_list* cur = list.list.head;
    while(all_result && cur) {
        struct shard_value result = shard_eval_call(e, pred, cur->value, e->error_scope->loc);
        all_result = result.type == SHARD_VAL_BOOL && result.boolean;
        cur = cur->next;
    }

    return BOOL_VAL(all_result);
}

static struct shard_value builtin_any(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value pred = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value list = shard_builtin_eval_arg(e, builtin, args, 1);

    bool any_result = false;
    struct shard_list* cur = list.list.head;
    while(!any_result && cur) {
        struct shard_value result = shard_eval_call(e, pred, cur->value, e->error_scope->loc);
        any_result = result.type == SHARD_VAL_BOOL && result.boolean;
        cur = cur->next;
    }

    return BOOL_VAL(any_result);
}

static struct shard_value builtin_find(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value pred = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value list = shard_builtin_eval_arg(e, builtin, args, 1);

    bool any_result = false;
    struct shard_list* cur = list.list.head;
    while(!any_result && cur) {
        struct shard_value result = shard_eval_call(e, pred, cur->value, e->error_scope->loc);
        if(result.type == SHARD_VAL_BOOL && result.boolean)
            return shard_eval_lazy2(e, cur->value);

        cur = cur->next;
    }

    return NULL_VAL();
}

static struct shard_value builtin_filter(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value pred = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value list = shard_builtin_eval_arg(e, builtin, args, 1);

    struct shard_list* cur = list.list.head, *result, *result_head = NULL, *result_cur = NULL;
    while(cur) {
        struct shard_value filter_result = shard_eval_call(e, pred, cur->value, e->error_scope->loc);
        if(filter_result.type != SHARD_VAL_BOOL || !filter_result.boolean)
            goto next;

        result = shard_gc_malloc(e->gc, sizeof(struct shard_list));
        result->next = NULL;
        result->value = cur->value;

        if(!result_head)
            result_head = result;
        
        if(result_cur)
            result_cur->next = result;

        result_cur = result;

    next:
        cur = cur->next;
    }

    return LIST_VAL(result_head);
}

static struct shard_value builtin_filterAttrs(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value pred = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value set = shard_builtin_eval_arg(e, builtin, args, 1);

    struct shard_set* result = shard_set_init(e->ctx, set.set->size);
    for(size_t i = 0; i < set.set->capacity; i++) {
        if(!set.set->entries[i].key)
            continue;

        struct shard_value inner_pred = shard_eval_call(e, pred, shard_unlazy(e->ctx, CSTRING_VAL(set.set->entries[i].key)), e->error_scope->loc);
        if(!(inner_pred.type & SHARD_VAL_CALLABLE))
            shard_eval_throw(e, e->error_scope->loc, "predicate function passed to `builtins.filterAttrs` is expected to take two arguments");

        struct shard_value filter_result = shard_eval_call(e, inner_pred, set.set->entries[i].value, e->error_scope->loc);
        if(filter_result.type != SHARD_VAL_BOOL || !filter_result.boolean)
            continue;

        shard_set_put(result, set.set->entries[i].key, set.set->entries[i].value);
    }

    return SET_VAL(result);
}

static struct shard_value builtin_defaultFunctionArg(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value arg_name = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value func = shard_builtin_eval_arg(e, builtin, args, 1);

    shard_ident_t arg_ident = shard_get_ident(e->ctx, arg_name.string);
    struct shard_pattern *arg_pattern = func.function.arg;

    for(size_t i = 0; i < arg_pattern->attrs.count; i++) {
        struct shard_binding *binding = arg_pattern->attrs.items + i;

        if(binding->ident == arg_ident)
            return binding->value ? 
                shard_eval_lazy2(e, shard_lazy(e->ctx, binding->value, func.function.scope))
                : NULL_VAL();
    }

    return NULL_VAL();
}

static struct shard_value builtin_functionArgs(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value func = shard_builtin_eval_arg(e, builtin, args, 0);

    struct shard_pattern *arg_pattern = func.function.arg;
    
    struct shard_set *arg_set = shard_set_init(e->ctx, arg_pattern->attrs.count);
    
    for(size_t i = 0; i < arg_pattern->attrs.count; i++) {
        struct shard_binding *binding = arg_pattern->attrs.items + i;
        shard_set_put(arg_set, binding->ident, shard_unlazy(e->ctx, BOOL_VAL(binding->value != NULL)));
    }

    return SET_VAL(arg_set);
}

static struct shard_value builtin_map(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value func = shard_builtin_eval_arg(e, builtin, args, 0);

    struct shard_value list = shard_builtin_eval_arg(e, builtin, args, 1);

    struct shard_list *result = NULL, *result_head = NULL, *result_last = NULL;
    struct shard_list *cur = list.list.head;

    while(cur) {
        struct shard_value elem = shard_eval_call(e, func, cur->value, e->error_scope->loc);

        result = shard_gc_malloc(e->gc, sizeof(struct shard_list));
        result->value = shard_unlazy(e->ctx, elem);
        result->next = NULL;

        if(!result_head)
            result_head = result;

        if(result_last)
            result_last->next = result;

        result_last = result;
        cur = cur->next;
    }

    return LIST_VAL(result_head);
}

static struct shard_value builtin_mapAttrs(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value func = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value set = shard_builtin_eval_arg(e, builtin, args, 1);

    struct shard_set *result = shard_set_init(e->ctx, set.set->size);

    for(size_t i = 0; i < set.set->capacity; i++) {
        shard_ident_t key = set.set->entries[i].key;
        if(!key)
            continue;

        struct shard_lazy_value *val = set.set->entries[i].value;

        struct shard_value func2 = shard_eval_call(e, func, shard_unlazy(e->ctx, CSTRING_VAL(key)), e->error_scope->loc);
        if(!(func2.type & SHARD_VAL_CALLABLE))
            shard_eval_throw(e, e->error_scope->loc, "builtins.mapAttrs: Mapper function is required to take in two arguments");

        struct shard_value mapped = shard_eval_call(e, func2, val, e->error_scope->loc);

        shard_set_put(result, key, shard_unlazy(e->ctx, mapped));
    }

    return SET_VAL(result);
}

static struct shard_value builtin_split(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value delim = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value string = shard_builtin_eval_arg(e, builtin, args, 1);

    char* string_copy = shard_gc_malloc(e->gc, (string.strlen + 1) * sizeof(char));
    memcpy(string_copy, string.string, (string.strlen + 1) * sizeof(char));

    struct shard_list *head = NULL, *prev = NULL, *next = NULL;

    char* sect = strtok(string_copy, delim.string);
    while(sect != NULL) {
        next = shard_gc_malloc(e->gc, sizeof(struct shard_list));
        next->value = shard_unlazy(e->ctx, STRING_VAL(sect, strlen(sect)));
        next->next = NULL;

        if(!head)
            head = next;

        if(prev)
            prev->next = next;

        prev = next;
        sect = strtok(NULL, delim.string);
    }

    return LIST_VAL(head);
}

static struct shard_value builtin_splitFirst(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value delim = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value string = shard_builtin_eval_arg(e, builtin, args, 1);

    char* string_copy = shard_gc_malloc(e->gc, (string.strlen + 1) * sizeof(char));
    memcpy(string_copy, string.string, (string.strlen + 1) * sizeof(char));

    char* split = strstr(string.string, delim.string);


    struct shard_list *head = shard_gc_malloc(e->gc, sizeof(struct shard_list));
    if(!split) {
        head->next = NULL;
        head->value = args[1];
        return LIST_VAL(head);
    }

    struct shard_list* tail = shard_gc_malloc(e->gc, sizeof(struct shard_list));
    tail->next = NULL;
    head->next = tail;

    const char* first = shard_gc_strdup(e->gc, string.string, split - string.string);
    const char* second = shard_gc_strdup(e->gc, split + delim.strlen, string.strlen - (split - string.string) - delim.strlen);

    head->value = shard_unlazy(e->ctx, STRING_VAL(first, strlen(first)));
    tail->value = shard_unlazy(e->ctx, STRING_VAL(second, strlen(second)));

    return LIST_VAL(head);
}

static struct shard_value builtin_abort(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);
    shard_eval_throw(e, e->error_scope->loc, "evaluation aborted with the following message: '%.*s'", (int) arg.strlen, arg.string);
}

static struct shard_value builtin_attrNames(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);

    struct shard_list *head = NULL, *current = NULL, *next;
    for(size_t i = 0; i < arg.set->capacity; i++) {
        if(!arg.set->entries[i].key)
            continue;

        next = shard_gc_malloc(e->gc, sizeof(struct shard_list));
        next->value = shard_unlazy(e->ctx, CSTRING_VAL(arg.set->entries[i].key));
        next->next = NULL;

        if(current)
            current->next = next;
        current = next;
        if(!head)
            head = current;
    }

    // TODO: sort alphabetically
    return LIST_VAL(head);
}

static struct shard_value builtin_attrValues(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);

    struct shard_list *head = NULL, *current = NULL, *next;
    for(size_t i = 0; i < arg.set->capacity; i++) {
        if(!arg.set->entries[i].key)
            continue;

        next = shard_gc_malloc(e->gc, sizeof(struct shard_list));
        next->value = arg.set->entries[i].value;
        next->next = NULL;

        if(current)
            current->next = next;
        current = next;
        if(!head)
            head = current;
    }

    // TODO: sort after keys alphabetically
    return LIST_VAL(head);
}

static struct shard_value builtin_basename(volatile struct shard_evaluator *e, struct shard_builtin *builtin, struct shard_lazy_value** args) {
    struct shard_value path = shard_builtin_eval_arg(e, builtin, args, 0);

    char *base = shard_gc_strdup(e->gc, path.path, path.pathlen);
    base = basename(base);

    return CPATH_VAL(base);
}

static struct shard_value builtin_dirname(volatile struct shard_evaluator *e, struct shard_builtin *builtin, struct shard_lazy_value** args) {
    struct shard_value path = shard_builtin_eval_arg(e, builtin, args, 0);

    char *dir = shard_gc_strdup(e->gc, path.path, path.pathlen);
    dir = dirname(dir);

    return CPATH_VAL(dir);
}

static struct shard_value merge_set(volatile struct shard_evaluator* e, struct shard_set* fst, struct shard_set* snd) {
    struct shard_set* dest = shard_set_init(e->ctx, fst->size + snd->size);

    size_t i;
    for(i = 0; i < fst->capacity; i++) {
        if(fst->entries[i].key)
            shard_set_put(dest, fst->entries[i].key, fst->entries[i].value);
    }

    for(i = 0; i < snd->capacity; i++) {
        if(snd->entries[i].key) {

            struct shard_lazy_value* fst_attr, *snd_attr = snd->entries[i].value;
            struct shard_value fst_attr_value, snd_attr_value;
            if(shard_set_get(dest, snd->entries[i].key, &fst_attr) == 0 && 
                    (fst_attr_value = shard_eval_lazy2(e, fst_attr)).type == SHARD_VAL_SET &&
                    (snd_attr_value = shard_eval_lazy2(e, snd_attr)).type == SHARD_VAL_SET) {
                shard_set_put(dest, snd->entries[i].key, shard_unlazy(e->ctx, merge_set(e, fst_attr_value.set, snd_attr_value.set)));
            }
            else
                shard_set_put(dest, snd->entries[i].key, snd->entries[i].value);
        }
    }

    shard_update_set_scopes(e, dest, fst);
    shard_update_set_scopes(e, dest, snd);

    return SET_VAL(dest);
}

static struct shard_value builtin_mergeTree(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value a = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value b = shard_builtin_eval_arg(e, builtin, args, 1);

    return merge_set(e, a.set, b.set);
}

static struct shard_value builtin_bitAnd(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value a = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value b = shard_builtin_eval_arg(e, builtin, args, 1);

    return INT_VAL(a.integer & b.integer);
}

static struct shard_value builtin_bitOr(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value a = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value b = shard_builtin_eval_arg(e, builtin, args, 1);

    return INT_VAL(a.integer | b.integer);
}

static struct shard_value builtin_bitXor(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value a = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value b = shard_builtin_eval_arg(e, builtin, args, 1);

    return INT_VAL(a.integer ^ b.integer);
}

static struct shard_value builtin_catAttrs(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value attr = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value list = shard_builtin_eval_arg(e, builtin, args, 1);

    shard_ident_t attr_ident = shard_get_ident(e->ctx, attr.string);

    struct shard_list *head = NULL, *next, *last = NULL;

    struct shard_list* list_cur = list.list.head;
    while(list_cur) {
        struct shard_value elem = shard_eval_lazy2(e, list_cur->value);
        struct shard_lazy_value* set_attr;
        if(elem.type != SHARD_VAL_SET || shard_set_get(elem.set, attr_ident, &set_attr))
            goto iter_continue;

        next = shard_gc_malloc(e->gc, sizeof(struct shard_list));
        next->next = NULL;
        next->value = set_attr;
        
        if(last)
            last->next = next;
        if(!head)
            head = next;
        last = next;

    iter_continue:
        list_cur = list_cur->next;
    }

    return LIST_VAL(head);
}

static struct shard_value builtin_foldAttrs(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value op = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value set = shard_builtin_eval_arg(e, builtin, args, 2);

    struct shard_lazy_value* result = args[1];
    
    for(size_t i = 0; i < set.set->capacity; i++) {
        if(!set.set->entries[i].key)
            continue;

        struct shard_value op2 = shard_eval_call(e, op, result, e->error_scope->loc);
        if(!(op2.type & SHARD_VAL_CALLABLE))
            shard_eval_throw(e, e->error_scope->loc, "`builtins.foldAttrs` expects first argument to take three call arguments");

        struct shard_lazy_value *attr_name = shard_unlazy(e->ctx, CSTRING_VAL(set.set->entries[i].key));
        struct shard_value op3 = shard_eval_call(e, op2, attr_name, e->error_scope->loc);
        if(!(op3.type & SHARD_VAL_CALLABLE))
            shard_eval_throw(e, e->error_scope->loc, "`builtins.foldAttrs` expects first argument to take three call arguments");

        struct shard_lazy_value *attr_value = set.set->entries[i].value;
        result = shard_unlazy(e->ctx, shard_eval_call(e, op3, attr_value, e->error_scope->loc));
    }

    return shard_eval_lazy2(e, result);
}

static struct shard_value builtin_foldl(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value op = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value list = shard_builtin_eval_arg(e, builtin, args, 2);

    struct shard_lazy_value* result = args[1];
    struct shard_list* list_cur = list.list.head;

    while(list_cur) {    
        struct shard_value op2 = shard_eval_call(e, op, result, e->error_scope->loc);
        if(!(op2.type & SHARD_VAL_CALLABLE))
            shard_eval_throw(e, e->error_scope->loc, "`builtins.foldl` expects first argument to take two call arguments");

        result = shard_unlazy(e->ctx, shard_eval_call(e, op2, list_cur->value, e->error_scope->loc));

        list_cur = list_cur->next;
    }

    return shard_eval_lazy2(e, result);
}

static struct shard_value builtin_genList(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value generator = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value length = shard_builtin_eval_arg(e, builtin, args, 1);

    struct shard_list *head = NULL, *next, *last = NULL;
    for(int64_t i = 0; i < length.integer; i++) {
        next = shard_gc_malloc(e->gc, sizeof(struct shard_list));
        next->next = NULL;
        next->value = shard_unlazy(e->ctx, 
            shard_eval_call(e, generator, shard_unlazy(e->ctx, INT_VAL(i)), e->error_scope->loc)
        );

        if(last)
            last->next = next;
        if(!head)
            head = next;
        last = next;
    }

    return LIST_VAL(head);
}

static struct shard_value builtin_getAttr(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value attr_name = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value set = shard_builtin_eval_arg(e, builtin, args, 1);

    struct shard_lazy_value* attr_lazy;
    int err = shard_set_get(set.set, shard_get_ident(e->ctx, attr_name.string), &attr_lazy);
    if(err)
        return NULL_VAL();

    return shard_eval_lazy2(e, attr_lazy);
}

static struct shard_value builtin_hasAttr(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value attr_name = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value set = shard_builtin_eval_arg(e, builtin, args, 1);

    return BOOL_VAL(shard_set_get(set.set, shard_get_ident(e->ctx, attr_name.string), NULL) == 0);
}

static struct shard_value builtin_setAttr(volatile struct shard_evaluator* e, struct shard_builtin *builtin, struct shard_lazy_value** args) {
    struct shard_value attr_name = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value old_set = shard_builtin_eval_arg(e, builtin, args, 2);

    struct shard_set *new_set = shard_set_init(e->ctx, old_set.set->size + 1);
    for(size_t i = 0; i < old_set.set->capacity; i++) {
        if(!old_set.set->entries[i].key)
            continue;

        shard_set_put(new_set, old_set.set->entries[i].key, old_set.set->entries[i].value);
    }

    shard_set_put(new_set, shard_get_ident(e->ctx, attr_name.string), args[1]);
    return SET_VAL(new_set);
}

static struct shard_value builtin_concatLists(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value lists = shard_builtin_eval_arg(e, builtin, args, 0);

    struct shard_list *concat_head = NULL, *concat_next = NULL, *concat_prev = NULL;

    for(struct shard_list *cur_list = lists.list.head; cur_list; cur_list = cur_list->next) {
        struct shard_value list = shard_eval_lazy2(e, cur_list->value);
        if(list.type != SHARD_VAL_LIST)
            shard_eval_throw(e, e->error_scope->loc, "`builtins.concatLists` expects first argument to be a list of lists");

        for(struct shard_list* cur_item = list.list.head; cur_item; cur_item = cur_item->next) {
            concat_next = shard_gc_malloc(e->gc, sizeof(struct shard_list));
            concat_next->next = NULL;
            concat_next->value = cur_item->value;

            if(!concat_head)
                concat_head = concat_next;
            if(concat_prev)
                concat_prev->next = concat_next;

            concat_prev = concat_next;
        }
    }

    return LIST_VAL(concat_head);
}

static struct shard_value builtin_intersectAttrs(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value e1 = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value e2 = shard_builtin_eval_arg(e, builtin, args, 1);

    struct shard_set *result = shard_set_init(e->ctx, MIN(e1.set->size, e2.set->size));
    
    for(size_t i = 0; i < e2.set->capacity; i++) {
        shard_ident_t attr = e2.set->entries[i].key;
        if(!attr || shard_set_get(e1.set, attr, NULL))
            continue;

        shard_set_put(result, attr, e2.set->entries[i].value);
    }

    return SET_VAL(result);
}

static struct shard_value builtin_isAttrs(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    (void) builtin;
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);
    return BOOL_VAL(arg.type == SHARD_VAL_SET);
}

static struct shard_value builtin_isBool(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    (void) builtin;
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);
    return BOOL_VAL(arg.type == SHARD_VAL_BOOL);
}

static struct shard_value builtin_isFloat(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    (void) builtin;
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);
    return BOOL_VAL(arg.type == SHARD_VAL_FLOAT);
}

static struct shard_value builtin_isFunction(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    (void) builtin;
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);
    return BOOL_VAL(arg.type & (SHARD_VAL_FUNCTION | SHARD_VAL_BUILTIN));
}

static struct shard_value builtin_isInt(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    (void) builtin;
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);
    return BOOL_VAL(arg.type == SHARD_VAL_INT);
}

static struct shard_value builtin_isList(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    (void) builtin;
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);
    return BOOL_VAL(arg.type == SHARD_VAL_LIST);
}

static struct shard_value builtin_isNull(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    (void) builtin;
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);
    return BOOL_VAL(arg.type == SHARD_VAL_NULL);
}

static struct shard_value builtin_isPath(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    (void) builtin;
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);
    return BOOL_VAL(arg.type == SHARD_VAL_PATH);
}

static struct shard_value builtin_isString(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    (void) builtin;
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);
    return BOOL_VAL(arg.type == SHARD_VAL_STRING);
}

static struct shard_value builtin_join(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value intermediate = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value list = shard_builtin_eval_arg(e, builtin, args, 1);

    struct shard_string concat = {0};

    for(struct shard_list* cur = list.list.head; cur; cur = cur->next) {
        struct shard_value elem = shard_eval_lazy2(e, cur->value); 
        if(elem.type != SHARD_VAL_STRING)
            shard_eval_throw(e, e->error_scope->loc, "`builtins.list` expects second argument to be a list of strings");

        shard_gc_string_appendn(e->gc, &concat, elem.string, elem.strlen);

        if(cur->next)
            shard_gc_string_appendn(e->gc, &concat, intermediate.string, intermediate.strlen);
    }

    return STRING_VAL(EITHER(concat.items, ""), concat.count);
}

static struct shard_value builtin_length(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);

    int64_t i = 0;
    struct shard_list* current = arg.list.head;
    while(current) {
        current = current->next;
        i++;
    }

    return INT_VAL(i);
}

static struct shard_value builtin_toPath(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value str = shard_builtin_eval_arg(e, builtin, args, 0);
    return PATH_VAL(str.string, str.strlen);
}

static struct shard_value builtin_trim(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);

    const char* str = arg.string;

    size_t size = arg.strlen;
    if(size == 0)
        return STRING_VAL(arg.string, 0);

    for(int64_t i = size - 1; i >= 0; i--) {
        if(isspace(str[i]))
            size--;
        else
            break;
    }

    for(size_t i = 0; i < size; i++) {
        if(isspace(*str)) {
            str++;
            size--;
        }
        else
            break;
    }

    return CSTRING_VAL(shard_gc_strdup(e->gc, str, size));
}

static struct shard_value builtin_parseInt(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);

    long long int_val = strtoll(arg.string, NULL, 10);
    return INT_VAL((int64_t) int_val);
}

static struct shard_value builtin_toString(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);
    switch(arg.type) {
        case SHARD_VAL_STRING:
            return arg;
        case SHARD_VAL_NULL:
            return CSTRING_VAL("");
        case SHARD_VAL_PATH:
            return STRING_VAL(arg.path, arg.pathlen);
        case SHARD_VAL_BOOL:
            return arg.boolean ? CSTRING_VAL("1") : CSTRING_VAL("");
        case SHARD_VAL_INT: {
            char* buf = shard_gc_malloc(e->gc, 32);
            snprintf(buf, 32, "%ld", arg.integer);
            return CSTRING_VAL(buf);
        } break;
        case SHARD_VAL_LIST: {
            struct shard_string accum = {0};
            shard_gc_string_appendn(e->gc, &accum, "[", 1);

            for(struct shard_list* head = arg.list.head; head; head = head->next) {
                struct shard_value elem_str = builtin_toString(e, builtin, &head->value);
                assert(elem_str.type == SHARD_VAL_STRING);

                shard_gc_string_appendn(e->gc, &accum, elem_str.string, elem_str.strlen);
                shard_gc_string_appendn(e->gc, &accum, head->next ? " " : "]", 1);
            }

            return STRING_VAL(accum.items, accum.count);
        } break;
        case SHARD_VAL_SET: {
            struct shard_lazy_value* val;
            if(shard_set_get(arg.set, shard_get_ident(e->ctx, "__toString"), &val))
                shard_eval_throw(e, e->error_scope->loc, "`builtins.toString` cannot print set without `__toString` attribute");
            
            struct shard_value to_str = shard_eval_lazy2(e, val);
            if(!(to_str.type & (SHARD_VAL_BUILTIN | SHARD_VAL_FUNCTION)))
                shard_eval_throw(e, e->error_scope->loc, "`builtins.toString` `__toString` attribute must be of type function");

            struct shard_value returned = shard_eval_call(e, to_str, shard_unlazy(e->ctx, arg), e->error_scope->loc);
            if(returned.type != SHARD_VAL_STRING)
                shard_eval_throw(e, e->error_scope->loc, "`builtins.toString` `__toString` must return a string");

            return returned;
        } break;
        default:
            shard_eval_throw(e, e->error_scope->loc, "`builtins.toString` cannot convert value of type `%s` to string", shard_value_type_to_string(e->ctx, arg.type));
    }
}

static struct shard_value builtin_tryEval(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    (void) builtin;
    shard_eval_lazy(e->ctx, *args);

    struct shard_value success, value;
    if((*args)->evaluated) {
        success = BOOL_VAL(true);
        value = (*args)->eval;
    }
    else
        value = success = BOOL_VAL(false);

    struct shard_set* set = shard_set_init(e->ctx, 2);
    shard_set_put(set, shard_get_ident(e->ctx, "success"), shard_unlazy(e->ctx, success));
    shard_set_put(set, shard_get_ident(e->ctx, "value"), shard_unlazy(e->ctx, value));

    return SET_VAL(set);
}

static struct shard_value builtin_tryGetLocation(volatile struct shard_evaluator* e, struct shard_builtin*, struct shard_lazy_value** args)  {
    struct shard_lazy_value* arg = args[0];

    struct shard_location loc;
    if(!arg->evaluated) {
        loc = arg->lazy->loc;
        shard_eval_lazy2(e, arg);
    }
    else if(arg->eval.type != SHARD_VAL_FUNCTION)
        return NULL_VAL();

    if(arg->eval.type == SHARD_VAL_FUNCTION)
        loc = arg->eval.function.body->loc;

    struct shard_set* set = shard_set_init(e->ctx, 3);
    shard_set_put(set, shard_get_ident(e->ctx, "file"), shard_unlazy(e->ctx, STRING_VAL(loc.src->origin, strlen(loc.src->origin))));
    shard_set_put(set, shard_get_ident(e->ctx, "line"), shard_unlazy(e->ctx, INT_VAL(loc.line)));
    shard_set_put(set, shard_get_ident(e->ctx, "column"), shard_unlazy(e->ctx, INT_VAL(loc.column)));

    return SET_VAL(set);
}

static struct shard_value builtin_throw(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value message = shard_builtin_eval_arg(e, builtin, args, 0);

    struct shard_error_scope *err_scope = e->error_scope;
    if(err_scope->prev)
        err_scope = err_scope->prev;

    shard_eval_throw(e, err_scope->loc, "%s", message.string);
}

static struct shard_value builtin_typeOf(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value val = shard_builtin_eval_arg(e, builtin, args, 0);
    return CSTRING_VAL(shard_value_type_to_string(e->ctx, val.type));
}

static struct shard_value builtin_head(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value val = shard_builtin_eval_arg(e, builtin, args, 0);

    if(!val.list.head)
        return NULL_VAL();
    return shard_eval_lazy2(e, val.list.head->value);
}

static struct shard_value builtin_tail(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value val = shard_builtin_eval_arg(e, builtin, args, 0);

    if(!val.list.head)
        return LIST_VAL(NULL);
    return LIST_VAL(val.list.head->next);
}

static struct shard_value builtin_seq(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    (void) builtin;
    shard_builtin_eval_arg(e, builtin, args, 0);
    return shard_builtin_eval_arg(e, builtin, args, 1);
}

static struct shard_value builtin_seqList(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value list = shard_builtin_eval_arg(e, builtin, args, 0);

    struct shard_list* list_cur = list.list.head;
    struct shard_list* last = NULL;

    while(list_cur) {
        if(last)
            shard_eval_lazy2(e, last->value);
        last = list_cur;
        list_cur = list_cur->next;
    }

    return last ? shard_eval_lazy2(e, last->value) : NULL_VAL();
}

static struct shard_value builtin_currentTime(volatile struct shard_evaluator* e, struct shard_builtin*, struct shard_lazy_value**) {
    (void) e;
    return INT_VAL(time(NULL));
}

static struct shard_value builtin_shardPath(volatile struct shard_evaluator* e, struct shard_builtin*, struct shard_lazy_value**) {
    struct shard_list *head = NULL, *current = NULL, *next;
    
    for(size_t i = 0; i < e->ctx->include_dirs.count; i++) {
        next = shard_gc_malloc(e->gc, sizeof(struct shard_list));
        next->next = NULL;
        next->value = shard_unlazy(e->ctx, CPATH_VAL(e->ctx->include_dirs.items[i]));

        if(current)
            current->next = next;
        current = next;
        if(!head)
            head = current;
    }

    return LIST_VAL(head);
}

static struct shard_value builtin_serialize(volatile struct shard_evaluator* e, struct shard_builtin *builtin, struct shard_lazy_value** args) {
    struct shard_value val = shard_builtin_eval_arg(e, builtin, args, 0);

    struct shard_string buffer = SHARD_DYNARR_EMPTY;

    shard_serialize2(e, &buffer, val);

    return STRING_VAL(buffer.items, buffer.count);
}

static struct shard_value builtin_import(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value val = shard_builtin_eval_arg(e, builtin, args, 0); 
    const char* filepath;

    switch(val.type) {
        case SHARD_VAL_STRING:
            filepath = val.string;
            break;
        case SHARD_VAL_PATH:
            filepath = val.path;
            break;
        default:
            assert(!"Unreachable");
    }

    struct shard_open_source* source = shard_open(e->ctx, filepath);
    if(!source)
        shard_eval_throw(e, e->error_scope->loc, "could not open `%s`: %s", filepath, strerror(errno));

    int err = shard_eval(e->ctx, source);
    if(err)
        shard_eval_throw(e, e->error_scope->loc, "error in `builtins.import` file `%s`.", filepath);

    return source->result;
}

static struct shard_value builtin_when(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value condition = shard_builtin_eval_arg(e, builtin, args, 0);
    if(condition.type != SHARD_VAL_BOOL)
        shard_eval_throw(e, e->error_scope->loc, "`builtins.when` expects condition to be of type bool");

    if(!condition.boolean)
        return LIST_VAL(NULL);

    struct shard_value value = shard_builtin_eval_arg(e, builtin, args, 1);
    if(value.type == SHARD_VAL_LIST)
        return value;
    else {
        struct shard_list* head = shard_gc_malloc(e->gc, sizeof(struct shard_list));
        head->value = shard_unlazy(e->ctx, value);
        head->next = NULL;
        return LIST_VAL(head);
    }
}

static struct shard_value builtin_while(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value func = shard_builtin_eval_arg(e, builtin, args, 0);

    struct shard_lazy_value* arg = shard_unlazy(e->ctx, NULL_VAL());
    struct shard_value cond;

    do {
        cond = shard_eval_call(e, func, arg, e->error_scope->loc);
    } while(cond.type == SHARD_VAL_BOOL && cond.boolean);

    return NULL_VAL();
}

#define IMPORT_BUILTIN 0

struct shard_builtin shard_builtin_toString = SHARD_BUILTIN("builtins.toString", builtin_toString, SHARD_VAL_ANY);
struct shard_builtin shard_builtin_seq = SHARD_BUILTIN("builtins.seq", builtin_seq, SHARD_VAL_ANY, SHARD_VAL_ANY);

static struct shard_builtin builtins[] = {
    [IMPORT_BUILTIN] = SHARD_BUILTIN("builtins.import", builtin_import, SHARD_VAL_TEXTUAL),

    SHARD_BUILTIN("builtins.abort", builtin_abort, SHARD_VAL_STRING),
    SHARD_BUILTIN("builtins.add", builtin_add, SHARD_VAL_NUMERIC | SHARD_VAL_TEXTUAL, SHARD_VAL_NUMERIC | SHARD_VAL_TEXTUAL),
    SHARD_BUILTIN("builtins.all", builtin_all, SHARD_VAL_CALLABLE, SHARD_VAL_LIST),
    SHARD_BUILTIN("builtins.any", builtin_any, SHARD_VAL_CALLABLE, SHARD_VAL_LIST),
    SHARD_BUILTIN("builtins.attrNames", builtin_attrNames, SHARD_VAL_SET),
    SHARD_BUILTIN("builtins.attrValues", builtin_attrValues, SHARD_VAL_SET),
    SHARD_BUILTIN("builtins.basename", builtin_basename, SHARD_VAL_PATH),
    SHARD_BUILTIN("builtins.bitAnd", builtin_bitAnd, SHARD_VAL_INT, SHARD_VAL_INT),
    SHARD_BUILTIN("builtins.bitOr", builtin_bitOr, SHARD_VAL_INT, SHARD_VAL_INT),
    SHARD_BUILTIN("builtins.bitXor", builtin_bitXor, SHARD_VAL_INT, SHARD_VAL_INT),
    SHARD_BUILTIN("builtins.catAttrs", builtin_catAttrs, SHARD_VAL_STRING, SHARD_VAL_LIST),
    SHARD_BUILTIN("builtins.ceil", builtin_ceil, SHARD_VAL_FLOAT),
    SHARD_BUILTIN("builtins.char", builtin_char, SHARD_VAL_INT),
    SHARD_BUILTIN("builtins.charAt", builtin_charAt, SHARD_VAL_INT, SHARD_VAL_STRING),
    SHARD_BUILTIN("builtins.concatLists", builtin_concatLists, SHARD_VAL_LIST),
    SHARD_BUILTIN("builtins.defaultFunctionArg", builtin_defaultFunctionArg, SHARD_VAL_STRING, SHARD_VAL_FUNCTION),
    SHARD_BUILTIN("builtins.dirname", builtin_dirname, SHARD_VAL_PATH),
    SHARD_BUILTIN("builtins.div", builtin_div, SHARD_VAL_NUMERIC, SHARD_VAL_NUMERIC),
    SHARD_BUILTIN("builtins.elem", builtin_elem, SHARD_VAL_ANY, SHARD_VAL_LIST),
    SHARD_BUILTIN("builtins.elemAt", builtin_elemAt, SHARD_VAL_LIST, SHARD_VAL_INT),
    SHARD_BUILTIN("builtins.errnoString", builtin_errnoString, SHARD_VAL_INT),
    SHARD_BUILTIN("builtins.evalutated", builtin_evaluated, SHARD_VAL_ANY),
    SHARD_BUILTIN("builtins.floor", builtin_floor, SHARD_VAL_FLOAT),
    SHARD_BUILTIN("builtins.foldAttrs", builtin_foldAttrs, SHARD_VAL_CALLABLE, SHARD_VAL_ANY, SHARD_VAL_SET),
    SHARD_BUILTIN("builtins.foldl", builtin_foldl, SHARD_VAL_CALLABLE, SHARD_VAL_ANY, SHARD_VAL_LIST),
    SHARD_BUILTIN("builtins.find",  builtin_find, SHARD_VAL_CALLABLE, SHARD_VAL_LIST),
    SHARD_BUILTIN("builtins.filter", builtin_filter, SHARD_VAL_CALLABLE, SHARD_VAL_LIST),
    SHARD_BUILTIN("builtins.filterAttrs", builtin_filterAttrs, SHARD_VAL_CALLABLE, SHARD_VAL_SET),
    SHARD_BUILTIN("builtins.functionArgs", builtin_functionArgs, SHARD_VAL_FUNCTION),
    SHARD_BUILTIN("builtins.genList", builtin_genList, SHARD_VAL_CALLABLE, SHARD_VAL_INT),
    SHARD_BUILTIN("builtins.getAttr", builtin_getAttr, SHARD_VAL_STRING, SHARD_VAL_SET),
    SHARD_BUILTIN("builtins.hasAttr", builtin_hasAttr, SHARD_VAL_STRING, SHARD_VAL_SET),
    SHARD_BUILTIN("builtins.head", builtin_head, SHARD_VAL_LIST),
    SHARD_BUILTIN("builtins.intersectAttrs", builtin_intersectAttrs, SHARD_VAL_SET, SHARD_VAL_SET),
    SHARD_BUILTIN("builtins.isAttrs", builtin_isAttrs, SHARD_VAL_ANY),
    SHARD_BUILTIN("builtins.isBool", builtin_isBool, SHARD_VAL_ANY),
    SHARD_BUILTIN("builtins.isFloat", builtin_isFloat, SHARD_VAL_ANY),
    SHARD_BUILTIN("builtins.isFunction", builtin_isFunction, SHARD_VAL_ANY),
    SHARD_BUILTIN("builtins.isInt", builtin_isInt, SHARD_VAL_ANY),
    SHARD_BUILTIN("builtins.isList", builtin_isList, SHARD_VAL_ANY),
    SHARD_BUILTIN("builtins.isNull", builtin_isNull, SHARD_VAL_ANY),
    SHARD_BUILTIN("builtins.isPath", builtin_isPath, SHARD_VAL_ANY),
    SHARD_BUILTIN("builtins.isString", builtin_isString, SHARD_VAL_ANY),
    SHARD_BUILTIN("builtins.join", builtin_join, SHARD_VAL_STRING, SHARD_VAL_LIST),
    SHARD_BUILTIN("builtins.length", builtin_length, SHARD_VAL_LIST),
    SHARD_BUILTIN("builtins.map", builtin_map, SHARD_VAL_CALLABLE, SHARD_VAL_LIST),
    SHARD_BUILTIN("builtins.mapAttrs", builtin_mapAttrs, SHARD_VAL_CALLABLE, SHARD_VAL_SET),
    SHARD_BUILTIN("builtins.mergeTree", builtin_mergeTree, SHARD_VAL_SET, SHARD_VAL_SET),
    SHARD_BUILTIN("builtins.mul", builtin_mul, SHARD_VAL_NUMERIC, SHARD_VAL_NUMERIC),
    SHARD_BUILTIN("builtins.not", builtin_not, SHARD_VAL_BOOL),
    SHARD_BUILTIN("builtins.serialize", builtin_serialize, SHARD_VAL_ANY),
    SHARD_BUILTIN("builtins.setAttr", builtin_setAttr, SHARD_VAL_STRING, SHARD_VAL_ANY, SHARD_VAL_SET),
    SHARD_BUILTIN("builtins.seq", builtin_seq, SHARD_VAL_ANY, SHARD_VAL_ANY),
    SHARD_BUILTIN("builtins.seqList", builtin_seqList, SHARD_VAL_LIST),
    SHARD_BUILTIN("builtins.split", builtin_split, SHARD_VAL_STRING, SHARD_VAL_STRING),
    SHARD_BUILTIN("builtins.splitFirst", builtin_splitFirst, SHARD_VAL_STRING, SHARD_VAL_STRING),
    SHARD_BUILTIN("builtins.sub", builtin_sub, SHARD_VAL_NUMERIC, SHARD_VAL_NUMERIC),
    SHARD_BUILTIN("builtins.tail", builtin_tail, SHARD_VAL_LIST),
    SHARD_BUILTIN("builtins.toPath", builtin_toPath, SHARD_VAL_STRING),
    SHARD_BUILTIN("builtins.trim", builtin_trim, SHARD_VAL_STRING),
    SHARD_BUILTIN("builtins.parseInt", builtin_parseInt, SHARD_VAL_STRING),
    SHARD_BUILTIN("builtins.tryEval", builtin_tryEval, SHARD_VAL_ANY),
    SHARD_BUILTIN("builtins.tryGetLocation", builtin_tryGetLocation, SHARD_VAL_ANY),
    SHARD_BUILTIN("builtins.throw", builtin_throw, SHARD_VAL_STRING),
    SHARD_BUILTIN("builtins.typeOf", builtin_typeOf, SHARD_VAL_ANY),
    SHARD_BUILTIN("builtins.when", builtin_when, SHARD_VAL_BOOL, SHARD_VAL_ANY),
    SHARD_BUILTIN("builtins.while", builtin_while, SHARD_VAL_CALLABLE),
    SHARD_BUILTIN("builtins.toString", builtin_toString, SHARD_VAL_ANY),

    SHARD_BUILTIN_CONST("builtins.currentTime", builtin_currentTime),
    SHARD_BUILTIN_CONST("builtins.shardPath", builtin_shardPath),
};

void shard_get_builtins(struct shard_context* ctx, struct shard_scope* dest) {
    struct shard_builtin_const constants[] = {
        { "builtins.currentSystem", (ctx->current_system ? CSTRING_VAL(ctx->current_system) : NULL_VAL()) },
        { "builtins.shardVersion", (CSTRING_VAL(SHARD_VERSION)) },
    };

    struct shard_set* global_scope = shard_set_init(ctx, 64);

    dest->bindings = global_scope;
    dest->outer = NULL;
    ctx->builtin_initialized = true; 

    shard_set_put(global_scope, shard_get_ident(ctx, "true"),     shard_unlazy(ctx, BOOL_VAL(true)));
    shard_set_put(global_scope, shard_get_ident(ctx, "false"),    shard_unlazy(ctx, BOOL_VAL(false)));
    shard_set_put(global_scope, shard_get_ident(ctx, "null"),     shard_unlazy(ctx, NULL_VAL()));
    shard_set_put(global_scope, shard_get_ident(ctx, "import"),   shard_unlazy(ctx, BUILTIN_VAL(&builtins[IMPORT_BUILTIN])));

    for(size_t i = 0; i < LEN(constants); i++)
        assert(shard_define_constant(ctx, constants[i].full_name, constants[i].value) == 0);
    
    for(size_t i = 0; i < LEN(builtins); i++)
        assert(shard_define_builtin(ctx, &builtins[i]) == 0);
}

int shard_define(struct shard_context* ctx, shard_ident_t ident, struct shard_lazy_value* value) {
    if(!ctx->builtin_initialized)
        shard_get_builtins(ctx, &ctx->builtin_scope);
    if(!ctx->builtin_initialized || !ident)
        return EINVAL;

    int res = 0;

    char* ident_copy = ctx->malloc((strlen(ident) + 1) * sizeof(char));
    memcpy(ident_copy, ident, (strlen(ident) + 1) * sizeof(char));

    char *next, *last = strtok(ident_copy, ".");
    
    struct shard_set* namespace = ctx->builtin_scope.bindings;

    while((next = strtok(NULL, "."))) {
        if(strlen(next) == 0) 
            return EINVAL;

        struct shard_lazy_value* lazy_val;
        if(shard_set_get(namespace, shard_get_ident(ctx, last), &lazy_val) == 0) { 
            if(shard_eval_lazy(ctx, lazy_val)) {
                res = EINVAL;
                goto cleanup;
            }

            struct shard_value *next_namespace = &lazy_val->eval;
            if(next_namespace->type != SHARD_VAL_SET) {
                res = EEXIST;
                goto cleanup;
            }

            namespace = next_namespace->set;
        }
        else {
            struct shard_value next_namespace = SET_VAL(shard_set_init(ctx, 256)); // TODO: grow dynamically
            shard_set_put(namespace, shard_get_ident(ctx, last), shard_unlazy(ctx, next_namespace));
            
            namespace = next_namespace.set;
        }

        last = next;
    }
    
    shard_ident_t attr = shard_get_ident(ctx, last);

    if(shard_set_get(namespace, attr, NULL) == 0) {
        res = EEXIST;
        goto cleanup;
    }

    shard_set_put(namespace, attr, value);

cleanup:
    ctx->free(ident_copy);
    return res;
}

int shard_define_builtin(struct shard_context* ctx, struct shard_builtin* builtin) {
    return shard_define(ctx, builtin->full_ident, shard_unlazy(ctx, BUILTIN_VAL(builtin)));
}

int shard_define_constant(struct shard_context* ctx, shard_ident_t ident, struct shard_value value) {
    return shard_define(ctx, ident, shard_unlazy(ctx, value));
}

