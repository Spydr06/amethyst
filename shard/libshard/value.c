#define _LIBSHARD_INTERNAL
#include <libshard.h>

#include <stdio.h>

struct shard_value shard_value_copy(volatile struct shard_evaluator* e, struct shard_value val) {
    switch(val.type) {
        case SHARD_VAL_LIST:
            return val; // TODO: deep-copy
        case SHARD_VAL_SET:
            return val; // TODO: deep-copy
        case SHARD_VAL_BUILTIN: {
            if(val.builtin.num_queued_args) {
                size_t size = sizeof(void*) * val.builtin.num_expected_args;
                struct shard_lazy_value** copied_args = shard_gc_malloc(e->gc, size);
                memcpy(copied_args, val.builtin.queued_args, size);
                val.builtin.queued_args = copied_args;
            }
            return val;
        }
        default:
            return val;
    }
}

int shard_value_to_string(struct shard_context* ctx, struct shard_string* str, const struct shard_value* val, int max_depth) {
    static int indentation_depth = 0;
    static char buf[48];
    switch(val->type) {
        case SHARD_VAL_BOOL:
            if(val->boolean)
                dynarr_append_many(ctx, str, "true", 4);
            else
                dynarr_append_many(ctx, str, "false", 5);
            break;
        case SHARD_VAL_NULL:
            dynarr_append_many(ctx, str, "null", 4);
            break;
        case SHARD_VAL_INT:
            snprintf(buf, LEN(buf), "%ld", val->integer);
            dynarr_append_many(ctx, str, buf, strlen(buf));
            break;
        case SHARD_VAL_FLOAT:
            snprintf(buf, LEN(buf), "%f", val->floating);
            dynarr_append_many(ctx, str, buf, strlen(buf));
            break;
        case SHARD_VAL_STRING:
            dynarr_append(ctx, str, '"');
            dynarr_append_many(ctx, str, val->string, val->strlen);
            dynarr_append(ctx, str, '"');
            break;
        case SHARD_VAL_PATH:
            dynarr_append_many(ctx, str, val->path, strlen(val->path));
            break;
        case SHARD_VAL_FUNCTION:
            snprintf(buf, LEN(buf), "<function %tx>", (ptrdiff_t) val->function.body);
            dynarr_append_many(ctx, str, buf, strlen(buf));
            break;
        case SHARD_VAL_LIST: {
            dynarr_append(ctx, str, '[');
            dynarr_append(ctx, str, ' ');

            if(max_depth > 0) {
                struct shard_list* item = val->list.head;
                while(item) {
                    if(!item->value->evaluated)
                        assert(shard_eval_lazy(ctx, item->value) == 0);
                    shard_value_to_string(ctx, str, &item->value->eval, max_depth--);
                    dynarr_append(ctx, str, ' ');
                    item = item->next;
                }

            } 
            else if(val->list.head)
                dynarr_append_many(ctx, str, "... ", 4);
            
            dynarr_append(ctx, str, ']');
        } break;
        case SHARD_VAL_SET: {
            char line_sep = val->set->capacity > 2 ? '\n' : ' ';
            if(line_sep == '\n')
                indentation_depth++;
            dynarr_append(ctx, str, '{');

            dynarr_append(ctx, str, line_sep);

            for(size_t i = 0; i < val->set->capacity; i++) {
                if(!val->set->entries[i].key)
                    continue;

                for(int j = 0; line_sep == '\n' && j < indentation_depth; j++)
                    dynarr_append_many(ctx, str, "    ", 4);

                dynarr_append_many(ctx, str, val->set->entries[i].key, strlen(val->set->entries[i].key));
                dynarr_append_many(ctx, str, " = ", 3);

                if(!val->set->entries[i].value->evaluated && shard_eval_lazy(ctx, val->set->entries[i].value) != 0)
                    dynarr_append_many(ctx, str, "<error>", 7);
                else
                    shard_value_to_string(ctx, str, &val->set->entries[i].value->eval, max_depth--);

                dynarr_append(ctx, str, ';');
                dynarr_append(ctx, str, line_sep);
            }

            for(int j = 0; line_sep == '\n' && j < indentation_depth - 1; j++)
                    dynarr_append_many(ctx, str, "    ", 4);

            if(line_sep == '\n')
                indentation_depth--;
            dynarr_append(ctx, str, '}');
        } break;
        case SHARD_VAL_BUILTIN:
            snprintf(buf, LEN(buf), "<builtin %tx>", (ptrdiff_t) val->function.body);
            dynarr_append_many(ctx, str, buf, strlen(buf));
            break;
    }

    return ctx->errors.count;
}


