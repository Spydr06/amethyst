#include <string.h>
#define _LIBSHARD_INTERNAL
#include <libshard.h>

#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>

#define BUILTIN_EXPR(_callback) ((struct shard_expr) {.type = SHARD_EXPR_BUILTIN, .loc = {0}, .builtin.callback = (_callback) })

#define LAZY_STATIC_EXPR(name, ctor) \
    static struct shard_expr* __lazy_##name(void) {     \
        static struct shard_expr expr;                  \
        static bool initialized = false;                \
        if(!initialized) {                              \
            expr = (ctor);                              \
            initialized = true;                         \
        }                                               \
        return &expr;                                   \
    }

#define GET_LAZY(name) (__lazy_##name())

struct builtin {
    const char* const ident;
    struct shard_lazy_value* value;
};

static struct shard_value builtin_add(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    return shard_eval_addition(e,
        shard_eval_lazy2(e, args[0]),
        shard_eval_lazy2(e, args[1]),
        loc
    );
}

static struct shard_value builtin_sub(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    return shard_eval_subtraction(e,
        shard_eval_lazy2(e, args[0]),
        shard_eval_lazy2(e, args[1]),
        loc
    );
}

static struct shard_value builtin_mul(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    return shard_eval_multiplication(e,
        shard_eval_lazy2(e, args[0]),
        shard_eval_lazy2(e, args[1]),
        loc
    );
}

static struct shard_value builtin_div(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    return shard_eval_division(e,
        shard_eval_lazy2(e, args[0]),
        shard_eval_lazy2(e, args[1]),
        loc
    );
}

static struct shard_value builtin_evaluated(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    (void) e;
    (void) loc;
    return BOOL_VAL((*args)->evaluated);
}

static struct shard_value builtin_floor(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value arg = shard_eval_lazy2(e, *args);
    if(arg.type != SHARD_VAL_FLOAT)
        shard_eval_throw(e, *loc, "`builtins.floor` expects argument to be of type `float`");

    return INT_VAL((int64_t) arg.floating);
}

static struct shard_value builtin_ceil(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value arg = shard_eval_lazy2(e, *args);
    if(arg.type != SHARD_VAL_FLOAT)
        shard_eval_throw(e, *loc, "`builtins.ceil` expects argument to be of type `float`");

    return INT_VAL((int64_t) (arg.floating + 1.0));
}

static struct shard_value builtin_all(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value pred = shard_eval_lazy2(e, args[0]);
    if(!(pred.type & SHARD_VAL_CALLABLE))
        shard_eval_throw(e, *loc, "`builtins.all` expects first argument to be of type `function`");

    struct shard_value list = shard_eval_lazy2(e, args[1]);
    if(list.type != SHARD_VAL_LIST)
        shard_eval_throw(e, *loc, "`builtins.all` expects second argument to be of type `list`");

    bool all_result = true;
    struct shard_list* cur = list.list.head;
    while(all_result && cur) {
        struct shard_value result = shard_eval_call(e, pred, cur->value, *loc);
        all_result = result.type == SHARD_VAL_BOOL && result.boolean;
        cur = cur->next;
    }

    return BOOL_VAL(all_result);
}

static struct shard_value builtin_any(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value pred = shard_eval_lazy2(e, args[0]);
    if(!(pred.type & SHARD_VAL_CALLABLE))
        shard_eval_throw(e, *loc, "`builtins.any` expects first argument to be callable");

    struct shard_value list = shard_eval_lazy2(e, args[1]);
    if(list.type != SHARD_VAL_LIST)
        shard_eval_throw(e, *loc, "`builtins.any` expects second argument to be of type `list`");

    bool any_result = false;
    struct shard_list* cur = list.list.head;
    while(!any_result && cur) {
        struct shard_value result = shard_eval_call(e, pred, cur->value, *loc);
        any_result = result.type == SHARD_VAL_BOOL && result.boolean;
        cur = cur->next;
    }

    return BOOL_VAL(any_result);
}

static struct shard_value builtin_map(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value func = shard_eval_lazy2(e, args[0]);
    if(!(func.type & SHARD_VAL_CALLABLE))
        shard_eval_throw(e, *loc, "`builtins.map` expects first argument to be callable");

    struct shard_value list = shard_eval_lazy2(e, args[1]);
    if(list.type != SHARD_VAL_LIST)
        shard_eval_throw(e, *loc, "`builtins.map` expects second argument to be of type `list`");

    struct shard_list *result = NULL, *result_head = NULL, *result_last = NULL;
    struct shard_list *cur = list.list.head;

    while(cur) {
        struct shard_value elem = shard_eval_call(e, func, cur->value, *loc);

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

static struct shard_value builtin_split(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value delim = shard_eval_lazy2(e, args[0]);
    if(delim.type != SHARD_VAL_STRING)
        shard_eval_throw(e, *loc, "`builtins.split` expects first argument to be of type `string`");

    struct shard_value string = shard_eval_lazy2(e, args[1]);
    if(string.type != SHARD_VAL_STRING)
        shard_eval_throw(e, *loc, "`builtins.split` expects second argument to be of type `string`");

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

static struct shard_value builtin_abort(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value arg = shard_eval_lazy2(e, *args);
    if(arg.type != SHARD_VAL_STRING)
        shard_eval_throw(e, *loc, "`builtins.abort` expects the argument to be of type `string`");

    shard_eval_throw(e, *loc, "evaluation aborted with the following message: '%.*s'", (int) arg.strlen, arg.string);
}

static struct shard_value builtin_attrNames(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value arg = shard_eval_lazy2(e, *args);
    if(arg.type != SHARD_VAL_SET)
        shard_eval_throw(e, *loc, "`builtins.attrNames` expects the argument to be of type `set`");

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

static struct shard_value builtin_attrValues(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value arg = shard_eval_lazy2(e, *args);
    if(arg.type != SHARD_VAL_SET)
        shard_eval_throw(e, *loc, "`builtins.attrValues` expects the argument to be of type `set`");

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

static struct shard_value builtin_mergeTree(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value a = shard_eval_lazy2(e, args[0]);
    if(a.type != SHARD_VAL_SET)
        shard_eval_throw(e, *loc, "`builtins.mergeTree` expects first argument to be of type `set`");

    struct shard_value b = shard_eval_lazy2(e, args[1]);
    if(b.type != SHARD_VAL_SET)
        shard_eval_throw(e, *loc, "`builtins.mergeTree` expects second argument to be of type `set`");

    return merge_set(e, a.set, b.set);
}

static struct shard_value builtin_bitAnd(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value a = shard_eval_lazy2(e, *args);
    if(a.type != SHARD_VAL_INT)
        shard_eval_throw(e, *loc, "`builtins.bitAnd` expects first argument to be of type `int`");

    struct shard_value b = shard_eval_lazy2(e, *args);
    if(b.type != SHARD_VAL_INT)
        shard_eval_throw(e, *loc, "`builtins.bitAnd` expects second argument to be of type `int`");

    return INT_VAL(a.integer & b.integer);
}

static struct shard_value builtin_bitOr(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value a = shard_eval_lazy2(e, *args);
    if(a.type != SHARD_VAL_INT)
        shard_eval_throw(e, *loc, "`builtins.bitOr` expects first argument to be of type `int`");

    struct shard_value b = shard_eval_lazy2(e, *args);
    if(b.type != SHARD_VAL_INT)
        shard_eval_throw(e, *loc, "`builtins.bitOr` expects second argument to be of type `int`");

    return INT_VAL(a.integer | b.integer);
}

static struct shard_value builtin_bitXor(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value a = shard_eval_lazy2(e, *args);
    if(a.type != SHARD_VAL_INT)
        shard_eval_throw(e, *loc, "`builtins.bitXor` expects first argument to be of type `int`");

    struct shard_value b = shard_eval_lazy2(e, *args);
    if(b.type != SHARD_VAL_INT)
        shard_eval_throw(e, *loc, "`builtins.bitXor` expects second argument to be of type `int`");

    return INT_VAL(a.integer ^ b.integer);
}

static struct shard_value builtin_catAttrs(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value attr = shard_eval_lazy2(e, args[0]);
    if(attr.type != SHARD_VAL_STRING)
        shard_eval_throw(e, *loc, "`builtins.catAttrs` expects first argument to be of type `string`");

    struct shard_value list = shard_eval_lazy2(e, args[1]);
    if(list.type != SHARD_VAL_LIST)
        shard_eval_throw(e, *loc, "`builtins.catAttrs` expects second argument to be of type `list` %d", attr.type);

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

static struct shard_value builtin_foldl(volatile struct shard_evaluator* e, struct shard_lazy_value** args,  struct shard_location* loc) {
    struct shard_value op = shard_eval_lazy2(e, args[0]);
    if(!(op.type & SHARD_VAL_CALLABLE))
        shard_eval_throw(e, *loc, "`builtins.foldl` expects first argument to be of type `function`");

    struct shard_value list = shard_eval_lazy2(e, args[2]);
    if(list.type != SHARD_VAL_LIST)
        shard_eval_throw(e, *loc, "`builtins.foldl` expects third argument to be of type `list`");

    struct shard_lazy_value* result = args[1];
    struct shard_list* list_cur = list.list.head;

    while(list_cur) {    
        struct shard_value op2 = shard_eval_call(e, op, result, *loc);
        if(!(op2.type & SHARD_VAL_CALLABLE))
            shard_eval_throw(e, *loc, "`builtins.foldl` expects first argument to take two call arguments");

        result = shard_unlazy(e->ctx, shard_eval_call(e, op2, list_cur->value, *loc));

        list_cur = list_cur->next;
    }

    return shard_eval_lazy2(e, result);
}

static struct shard_value builtin_genList(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value generator = shard_eval_lazy2(e, args[0]);
    if(!(generator.type & SHARD_VAL_CALLABLE))
        shard_eval_throw(e, *loc, "`builtins.genList` expects first argument to be of type `function`");

    struct shard_value length = shard_eval_lazy2(e, args[1]);
    if(length.type != SHARD_VAL_INT)
        shard_eval_throw(e, *loc, "`builtins.genList` expects second argument to be of type `int`, %d", length.type);

    struct shard_list *head = NULL, *next, *last = NULL;
    for(int64_t i = 0; i < length.integer; i++) {
        next = shard_gc_malloc(e->gc, sizeof(struct shard_list));
        next->next = NULL;
        next->value = shard_unlazy(e->ctx, 
            shard_eval_call(e, generator, shard_unlazy(e->ctx, INT_VAL(i)), *loc)
        );

        if(last)
            last->next = next;
        if(!head)
            head = next;
        last = next;
    }

    return LIST_VAL(head);
}

static struct shard_value builtin_concatLists(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value lists = shard_eval_lazy2(e, args[0]);
    if(lists.type != SHARD_VAL_LIST)
        shard_eval_throw(e, *loc, "`builtins.concatLists` expects first argument to be of type `list`");

    struct shard_list *concat_head = NULL, *concat_next = NULL, *concat_prev = NULL;

    for(struct shard_list *cur_list = lists.list.head; cur_list; cur_list = cur_list->next) {
        struct shard_value list = shard_eval_lazy2(e, cur_list->value);
        if(list.type != SHARD_VAL_LIST)
            shard_eval_throw(e, *loc, "`builtins.concatLists` expects first argument to be a list of lists");

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

static struct shard_value builtin_isAttrs(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    (void) loc;
    struct shard_value arg = shard_eval_lazy2(e, *args);
    return BOOL_VAL(arg.type == SHARD_VAL_SET);
}

static struct shard_value builtin_isBool(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    (void) loc;
    struct shard_value arg = shard_eval_lazy2(e, *args);
    return BOOL_VAL(arg.type == SHARD_VAL_BOOL);
}

static struct shard_value builtin_isFloat(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    (void) loc;
    struct shard_value arg = shard_eval_lazy2(e, *args);
    return BOOL_VAL(arg.type == SHARD_VAL_FLOAT);
}

static struct shard_value builtin_isFunction(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    (void) loc;
    struct shard_value arg = shard_eval_lazy2(e, *args);
    return BOOL_VAL(arg.type & (SHARD_VAL_FUNCTION | SHARD_VAL_BUILTIN));
}

static struct shard_value builtin_isInt(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    (void) loc;
    struct shard_value arg = shard_eval_lazy2(e, *args);
    return BOOL_VAL(arg.type == SHARD_VAL_INT);
}

static struct shard_value builtin_isList(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    (void) loc;
    struct shard_value arg = shard_eval_lazy2(e, *args);
    return BOOL_VAL(arg.type == SHARD_VAL_LIST);
}

static struct shard_value builtin_isNull(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    (void) loc;
    struct shard_value arg = shard_eval_lazy2(e, *args);
    return BOOL_VAL(arg.type == SHARD_VAL_NULL);
}

static struct shard_value builtin_isPath(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    (void) loc;
    struct shard_value arg = shard_eval_lazy2(e, *args);
    return BOOL_VAL(arg.type == SHARD_VAL_PATH);
}

static struct shard_value builtin_isString(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    (void) loc;
    struct shard_value arg = shard_eval_lazy2(e, *args);
    return BOOL_VAL(arg.type == SHARD_VAL_STRING);
}

static struct shard_value builtin_join(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value intermediate = shard_eval_lazy2(e, args[0]);
    if(intermediate.type != SHARD_VAL_STRING)
        shard_eval_throw(e, *loc, "`builtins.join` expects first argument to be of type string");

    struct shard_value list = shard_eval_lazy2(e, args[1]);
    if(list.type != SHARD_VAL_LIST)
        shard_eval_throw(e, *loc, "`builtins.list` expects second argument to be of type list");

    struct shard_string concat = {0};

    for(struct shard_list* cur = list.list.head; cur; cur = cur->next) {
        struct shard_value elem = shard_eval_lazy2(e, cur->value); 
        if(elem.type != SHARD_VAL_STRING)
            shard_eval_throw(e, *loc, "`builtins.list` expects second argument to be a list of strings");

        shard_gc_string_appendn(e->gc, &concat, elem.string, elem.strlen);

        if(cur->next)
            shard_gc_string_appendn(e->gc, &concat, intermediate.string, intermediate.strlen);
    }

    return STRING_VAL(EITHER(concat.items, ""), concat.count);
}

static struct shard_value builtin_length(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value arg = shard_eval_lazy2(e, *args);
    if(arg.type != SHARD_VAL_LIST)
        shard_eval_throw(e, *loc, "`builtins.length` expects list argument");

    int64_t i = 0;
    struct shard_list* current = arg.list.head;
    while(current) {
        current = current->next;
        i++;
    }

    return INT_VAL(i);
}

static struct shard_value builtin_toPath(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value str = shard_eval_lazy2(e, *args);
    if(str.type != SHARD_VAL_STRING)
        shard_eval_throw(e, *loc, "`builtins.toPath` expects argument to be of type `string`");

    return PATH_VAL(str.string, str.strlen);
}

static struct shard_value builtin_toString(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value arg = shard_eval_lazy2(e, *args);
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
                struct shard_value elem_str = builtin_toString(e, &head->value, loc);
                assert(elem_str.type == SHARD_VAL_STRING);

                shard_gc_string_appendn(e->gc, &accum, elem_str.string, elem_str.strlen);
                shard_gc_string_appendn(e->gc, &accum, head->next ? " " : "]", 1);
            }

            return STRING_VAL(accum.items, accum.count);
        } break;
        case SHARD_VAL_SET: {
            struct shard_lazy_value* val;
            if(shard_set_get(arg.set, shard_get_ident(e->ctx, "__toString"), &val))
                shard_eval_throw(e, *loc, "`builtins.toString` cannot print set without `__toString` attribute");
            
            struct shard_value to_str = shard_eval_lazy2(e, val);
            if(!(to_str.type & (SHARD_VAL_BUILTIN | SHARD_VAL_FUNCTION)))
                shard_eval_throw(e, *loc, "`builtins.toString` `__toString` attribute must be of type function");

            struct shard_value returned = shard_eval_call(e, to_str, shard_unlazy(e->ctx, arg), *loc);
            if(returned.type != SHARD_VAL_STRING)
                shard_eval_throw(e, *loc, "`builtins.toString` `__toString` must return a string");

            return returned;
        } break;
        default:
            shard_eval_throw(e, *loc, "`builtins.toString` cannot convert this value to string");
    }
}

static struct shard_value builtin_tryEval(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    (void) loc;
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

static struct shard_value builtin_typeOf(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value val = shard_eval_lazy2(e, *args);

    switch(val.type) {
        case SHARD_VAL_INT:
            return CSTRING_VAL("int");
        case SHARD_VAL_BOOL:
            return CSTRING_VAL("bool");
        case SHARD_VAL_FLOAT:
            return CSTRING_VAL("float");
        case SHARD_VAL_STRING:
            return CSTRING_VAL("string");
        case SHARD_VAL_PATH:
            return CSTRING_VAL("path");
        case SHARD_VAL_NULL:
            return CSTRING_VAL("null");
        case SHARD_VAL_SET:
            return CSTRING_VAL("set");
        case SHARD_VAL_LIST:
            return CSTRING_VAL("list");
        case SHARD_VAL_FUNCTION:
        case SHARD_VAL_BUILTIN:
            return CSTRING_VAL("lambda");
        default:
            shard_eval_throw(e, *loc, "`builtins.typeOf` unknown type `%d`", val.type);
    }
}

static struct shard_value builtin_head(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value val = shard_eval_lazy2(e, *args);
    if(val.type != SHARD_VAL_LIST)
        shard_eval_throw(e, *loc, "`builtins.head` expects a list");

    if(!val.list.head)
        return NULL_VAL();
    return shard_eval_lazy2(e, val.list.head->value);
}

static struct shard_value builtin_tail(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value val = shard_eval_lazy2(e, *args);
    if(val.type != SHARD_VAL_LIST)
        shard_eval_throw(e, *loc, "`builtins.tail` expects a list");

    if(!val.list.head)
        return LIST_VAL(NULL);
    return LIST_VAL(val.list.head->next);
}

static struct shard_value builtin_seq(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    (void) loc;
    shard_eval_lazy2(e, args[0]);
    return shard_eval_lazy2(e, args[1]);
}

static struct shard_value builtin_seqList(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value list = shard_eval_lazy2(e, *args);
    if(list.type != SHARD_VAL_LIST)
        shard_eval_throw(e, *loc, "`builtins.seqList` expects a list");

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

static struct shard_value builtin_currentTime(volatile struct shard_evaluator* e) {
    (void) e;
    return INT_VAL(time(NULL));
}

static struct shard_value builtin_shardPath(volatile struct shard_evaluator* e) {
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

static struct shard_value builtin_import(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value val = shard_eval_lazy2(e, *args); 
    const char* filepath;

    switch(val.type) {
        case SHARD_VAL_STRING:
            filepath = val.string;
            break;
        case SHARD_VAL_PATH:
            filepath = val.path;
            break;
        default:
            shard_eval_throw(e, *loc, "`builtins.import` expects a string or path");
    }

    struct shard_open_source* source = shard_open(e->ctx, filepath);
    if(!source)
        shard_eval_throw(e, *loc, "could not open `%s`: %s", filepath, strerror(errno));

    int err = shard_eval(e->ctx, source);
    if(err)
        shard_eval_throw(e, *loc, "error in `builtins.import` file `%s`.", filepath);

    return source->result;
}

static struct shard_value builtin_when(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value condition = shard_eval_lazy2(e, args[0]);
    if(condition.type != SHARD_VAL_BOOL)
        shard_eval_throw(e, *loc, "`builtins.when` expects condition to be of type bool");

    if(!condition.boolean)
        return LIST_VAL(NULL);

    struct shard_value value = shard_eval_lazy2(e, args[1]);
    if(value.type == SHARD_VAL_LIST)
        return value;
    else {
        struct shard_list* head = shard_gc_malloc(e->gc, sizeof(struct shard_list));
        head->value = shard_unlazy(e->ctx, value);
        head->next = NULL;
        return LIST_VAL(head);
    }
}

LAZY_STATIC_EXPR(currentTime, BUILTIN_EXPR(builtin_currentTime))
LAZY_STATIC_EXPR(shardPath, BUILTIN_EXPR(builtin_shardPath))

void shard_get_builtins(struct shard_context* ctx, struct shard_scope* dest) {
    struct shard_lazy_value* import = shard_unlazy(ctx, BUILTIN_VAL(builtin_import, 1));
    struct builtin builtins[] = {
        { "abort", shard_unlazy(ctx, BUILTIN_VAL(builtin_abort, 1)) },
        { "add", shard_unlazy(ctx, BUILTIN_VAL(builtin_add, 2)) },
        { "all", shard_unlazy(ctx, BUILTIN_VAL(builtin_all, 2)) },
        { "any", shard_unlazy(ctx, BUILTIN_VAL(builtin_any, 2)) },
        { "attrNames", shard_unlazy(ctx, BUILTIN_VAL(builtin_attrNames, 1)) },
        { "attrValues", shard_unlazy(ctx, BUILTIN_VAL(builtin_attrValues, 1)) },
        { "bitAnd", shard_unlazy(ctx, BUILTIN_VAL(builtin_bitAnd, 2)) },
        { "bitOr", shard_unlazy(ctx, BUILTIN_VAL(builtin_bitOr, 2)) },
        { "bitXor", shard_unlazy(ctx, BUILTIN_VAL(builtin_bitXor, 2)) },
        { "catAttrs", shard_unlazy(ctx, BUILTIN_VAL(builtin_catAttrs, 2)) },
        { "ceil", shard_unlazy(ctx, BUILTIN_VAL(builtin_ceil, 1)) },
        { "concatLists", shard_unlazy(ctx, BUILTIN_VAL(builtin_concatLists, 1)) },
        { "currentSystem", shard_unlazy(ctx, ctx->current_system ? CSTRING_VAL(ctx->current_system) : NULL_VAL()) },
        { "currentTime", shard_lazy(ctx, GET_LAZY(currentTime), NULL) },
        { "div", shard_unlazy(ctx, BUILTIN_VAL(builtin_div, 2)) },
        { "evaluated", shard_unlazy(ctx, BUILTIN_VAL(builtin_evaluated, 1)) },
        { "floor", shard_unlazy(ctx, BUILTIN_VAL(builtin_floor, 1)) },
        { "foldl", shard_unlazy(ctx, BUILTIN_VAL(builtin_foldl, 3)) },
        { "genList", shard_unlazy(ctx, BUILTIN_VAL(builtin_genList, 2)) },
        { "head", shard_unlazy(ctx, BUILTIN_VAL(builtin_head, 1)) },
        { "import", import },
        { "isAttrs", shard_unlazy(ctx, BUILTIN_VAL(builtin_isAttrs, 1)) },
        { "isBool", shard_unlazy(ctx, BUILTIN_VAL(builtin_isBool, 1)) },
        { "isFloat", shard_unlazy(ctx, BUILTIN_VAL(builtin_isFloat, 1)) },
        { "isFunction", shard_unlazy(ctx, BUILTIN_VAL(builtin_isFunction, 1)) },
        { "isInt", shard_unlazy(ctx, BUILTIN_VAL(builtin_isInt, 1)) },
        { "isList", shard_unlazy(ctx, BUILTIN_VAL(builtin_isList, 1)) },
        { "isNull", shard_unlazy(ctx, BUILTIN_VAL(builtin_isNull, 1)) },
        { "isPath", shard_unlazy(ctx, BUILTIN_VAL(builtin_isPath, 1)) },
        { "isString", shard_unlazy(ctx, BUILTIN_VAL(builtin_isString, 1)) },
        { "join", shard_unlazy(ctx, BUILTIN_VAL(builtin_join, 2)) },
        { "langVersion", shard_unlazy(ctx, INT_VAL(SHARD_VERSION_X)) },
        { "length", shard_unlazy(ctx, BUILTIN_VAL(builtin_length, 1)) },
        { "map", shard_unlazy(ctx, BUILTIN_VAL(builtin_map, 2)) },
        { "mergeTree", shard_unlazy(ctx, BUILTIN_VAL(builtin_mergeTree, 2)) },
        { "mul", shard_unlazy(ctx, BUILTIN_VAL(builtin_mul, 2)) },
        { "seq", shard_unlazy(ctx, BUILTIN_VAL(builtin_seq, 2)) },
        { "seqList", shard_unlazy(ctx, BUILTIN_VAL(builtin_seqList, 1)) },
        { "shardPath", shard_lazy(ctx, GET_LAZY(shardPath), NULL) },
        { "shardVersion", shard_unlazy(ctx, CSTRING_VAL(SHARD_VERSION)) },
        { "split", shard_unlazy(ctx, BUILTIN_VAL(builtin_split, 2)) },
        { "sub", shard_unlazy(ctx, BUILTIN_VAL(builtin_sub, 2)) },
        { "tail", shard_unlazy(ctx, BUILTIN_VAL(builtin_tail, 1)) },
        { "toPath", shard_unlazy(ctx, BUILTIN_VAL(builtin_toPath, 1)) },
        { "toString", shard_unlazy(ctx, BUILTIN_VAL(builtin_toString, 1)) },
        { "tryEval", shard_unlazy(ctx, BUILTIN_VAL(builtin_tryEval, 1)) },
        { "typeOf", shard_unlazy(ctx, BUILTIN_VAL(builtin_typeOf, 1)) },
        { "when", shard_unlazy(ctx, BUILTIN_VAL(builtin_when, 2)) },
    };

    struct shard_set* builtin_set = shard_set_init(ctx, LEN(builtins));

    for(size_t i = 0; i < LEN(builtins); i++) {
        shard_set_put(builtin_set, shard_get_ident(ctx, builtins[i].ident), builtins[i].value);
    }

    struct shard_set* global_scope = shard_set_init(ctx, 9);
    shard_set_put(global_scope, shard_get_ident(ctx, "true"),     shard_unlazy(ctx, BOOL_VAL(true)));
    shard_set_put(global_scope, shard_get_ident(ctx, "false"),    shard_unlazy(ctx, BOOL_VAL(false)));
    shard_set_put(global_scope, shard_get_ident(ctx, "null"),     shard_unlazy(ctx, NULL_VAL()));
    shard_set_put(global_scope, shard_get_ident(ctx, "import"),   import);
    shard_set_put(global_scope, shard_get_ident(ctx, "builtins"), shard_unlazy(ctx, SET_VAL(builtin_set)));

    dest->bindings = global_scope;
    dest->outer = NULL;
    
    ctx->builtin_initialized = true;    
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
            struct shard_value next_namespace = SET_VAL(shard_set_init(ctx, 32));
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

int shard_define_function(struct shard_context* ctx, shard_ident_t ident, struct shard_value (*callback)(volatile struct shard_evaluator*, struct shard_lazy_value**, struct shard_location*), unsigned arity) {
    return shard_define(ctx, ident, shard_unlazy(ctx, BUILTIN_VAL(callback, arity)));
}

int shard_define_constant(struct shard_context* ctx, shard_ident_t ident, struct shard_value value) {
    return shard_define(ctx, ident, shard_unlazy(ctx, value));
}

