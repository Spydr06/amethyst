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
                size_t size = sizeof(void*) * val.builtin.builtin->arity;
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
                    if(!item->value->evaluated) {
                        int err = shard_eval_lazy(ctx, item->value);
                        if(err)
                            return err;
                    }
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

bool shard_values_equal(struct shard_value* lhs, struct shard_value* rhs) {
    if(lhs->type != rhs->type)
        return false;

    switch(lhs->type) {
        case SHARD_VAL_NULL:
            return true;
        case SHARD_VAL_INT:
            return lhs->integer == rhs->integer;
        case SHARD_VAL_FLOAT:
            return lhs->floating == rhs->floating;
        case SHARD_VAL_BOOL:
            return lhs->boolean == rhs->boolean;
        case SHARD_VAL_PATH:
            return strcmp(lhs->path, rhs->path) == 0;
        case SHARD_VAL_STRING:
            return strcmp(lhs->string, rhs->string) == 0;
        case SHARD_VAL_FUNCTION:
            return memcmp(&lhs->function, &rhs->function, sizeof(lhs->function)) == 0;
        case SHARD_VAL_BUILTIN:
            return lhs->builtin.builtin == rhs->builtin.builtin;
        case SHARD_VAL_SET:
        case SHARD_VAL_LIST:
        default:
            assert(false && "unimplemented!");
    }
}

const char* shard_primitive_value_type_to_string(enum shard_value_type type) {
    switch(type) {
        case SHARD_VAL_NULL:
            return "null";
        case SHARD_VAL_INT:
            return "int";
        case SHARD_VAL_BOOL:
            return "bool";
        case SHARD_VAL_STRING:
            return "string";
        case SHARD_VAL_LIST:
            return "list";
        case SHARD_VAL_SET:
            return "set";
        case SHARD_VAL_FLOAT:
            return "float";
        case SHARD_VAL_FUNCTION:
            return "function";
        case SHARD_VAL_PATH:
            return "path";
        case SHARD_VAL_BUILTIN:
            return "builtin";
        default:
            return "<unknown>";
    }
}

const char* shard_value_type_to_string(struct shard_context* ctx, enum shard_value_type type) {
    if(type == SHARD_VAL_CALLABLE)
        return "callable";
    else if(type == SHARD_VAL_ANY)
        return "any";

    bool first = true;
    struct shard_string s = {0};

    for(unsigned i = 0; i < sizeof(enum shard_value_type) * 8; i++) {
        enum shard_value_type test = 1 << i;

        if(!(type & test))
            continue;

        if(!first)
            shard_gc_string_push(ctx->gc, &s, '|');

        const char* prim = shard_primitive_value_type_to_string(test);
        shard_gc_string_append(ctx->gc, &s, prim);

        first = false;
    }

    shard_gc_string_push(ctx->gc, &s, '\0');
    return s.items;
}

