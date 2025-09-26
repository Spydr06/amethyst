#include <libshard.h>

#include <ctype.h>
#include <string.h>

#include <stdio.h>

static void serialize_escaped_string(volatile struct shard_evaluator* e, struct shard_string* buffer, const char* str, size_t len) {
    shard_gc_string_push(e->gc, buffer, '"');

    for(size_t i = 0; i < len; i++) {
        char c = str[i];

        switch(c) {
            case '\t':
                shard_gc_string_appendn(e->gc, buffer, "\\t", 2);
                break;
            case '\"':
                shard_gc_string_appendn(e->gc, buffer, "\\\"", 2);
                break;
            case '\\':
                shard_gc_string_appendn(e->gc, buffer, "\\\\", 2);
                break;
            case '\n':
                shard_gc_string_appendn(e->gc, buffer, "\\n", 2);
                break;
            default:
                shard_gc_string_push(e->gc, buffer, c);
        }
    }

    shard_gc_string_push(e->gc, buffer, '"');
}

SHARD_DECL void shard_serialize2(volatile struct shard_evaluator* e, struct shard_string* buffer, struct shard_value value) {
    switch(value.type) {
        case SHARD_VAL_NULL:
            shard_gc_string_append(e->gc, buffer, "null");
            break;
        case SHARD_VAL_BOOL:
            if(value.boolean)
                shard_gc_string_append(e->gc, buffer, "true");
            else
                shard_gc_string_append(e->gc, buffer, "false");
            break;
        case SHARD_VAL_STRING:
            serialize_escaped_string(e, buffer, value.string, value.strlen);
            break;
        case SHARD_VAL_PATH:
            for(size_t i = 0; i < value.pathlen; i++) {
                char c = value.path[i];

                if(isspace(c))
                    shard_gc_string_push(e->gc, buffer, '\\');

                shard_gc_string_push(e->gc, buffer, c);
            }
            break;
        case SHARD_VAL_INT:
            char itos[100];
            snprintf(itos, sizeof(itos), "%ld", value.integer);

            shard_gc_string_append(e->gc, buffer, itos);
            break;
        case SHARD_VAL_FLOAT:
            char ftos[100];
            snprintf(ftos, sizeof(ftos), "%.17f", value.floating);

            shard_gc_string_append(e->gc, buffer, ftos);
            break;
        case SHARD_VAL_LIST:
            shard_gc_string_push(e->gc, buffer, '[');
            shard_gc_string_push(e->gc, buffer, ' ');

            struct shard_list *iter = value.list.head;

            while(iter) {
                struct shard_value elem = shard_eval_lazy2(e, iter->value);
                shard_serialize2(e, buffer, elem);

                shard_gc_string_push(e->gc, buffer, ' ');

                iter = iter->next;
            }

            shard_gc_string_push(e->gc, buffer, ']');
            break;
        case SHARD_VAL_SET:
            shard_gc_string_push(e->gc, buffer, '{');
            shard_gc_string_push(e->gc, buffer, ' ');

            for(size_t i = 0; i < value.set->capacity; i++) {
                shard_ident_t key = value.set->entries[i].key;
                struct shard_lazy_value* iter = value.set->entries[i].value;

                if(!key)
                    continue;

                if(shard_is_valid_identifier(key))
                    shard_gc_string_append(e->gc, buffer, key);
                else
                    serialize_escaped_string(e, buffer, key, strlen(key));

                shard_gc_string_appendn(e->gc, buffer, " = ", 3);

                struct shard_value elem = shard_eval_lazy2(e, iter);
                shard_serialize2(e, buffer, elem);

                shard_gc_string_push(e->gc, buffer, ';');
                shard_gc_string_push(e->gc, buffer, ' ');
            }

            shard_gc_string_push(e->gc, buffer, '}');
            break;
        case SHARD_VAL_BUILTIN:
        case SHARD_VAL_FUNCTION:
        default:
            shard_eval_throw(e, e->error_scope->loc, "`builtins.serialize` cannot serialize value of type `%s`", shard_value_type_to_string(e->ctx, value.type));
            break;
    }
}

