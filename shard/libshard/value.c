#define _LIBSHARD_INTERNAL
#include <libshard.h>

#include <stdio.h>

void shard_value_to_string(struct shard_context* ctx, struct shard_string* str, const struct shard_value* val, int max_depth) {
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
            dynarr_append_many(ctx, str, "<function>", 9);
            break;
        case SHARD_VAL_LIST: {
            dynarr_append(ctx, str, '[');
            dynarr_append(ctx, str, ' ');

            if(max_depth > 0) {
                struct shard_list* item = val->list.head;
                while(item) {
                    if(!item->value.evaluated)
                        assert(shard_eval_lazy(ctx, &item->value) == 0);
                    shard_value_to_string(ctx, str, &item->value.eval, max_depth--);
                    dynarr_append(ctx, str, ' ');
                    item = item->next;
                }

            } 
            else if(val->list.head)
                dynarr_append_many(ctx, str, "... ", 4);
            
            dynarr_append(ctx, str, ']');
        } break;
        case SHARD_VAL_SET: {
            dynarr_append(ctx, str, '{');
            dynarr_append(ctx, str, ' ');
            // TODO
            dynarr_append(ctx, str, '}');
        } break;
    }
}


