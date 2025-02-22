#include "x86_64/trace.h"
#include <sanitize/ub.h>
#include <kernelio.h>

#define is_aligned(value, alignment) (!((value) & ((alignment) - 1)))

#define ubsan_logf(loc, ...) (__ubsan_logf((loc), __func__, __VA_ARGS__))

const char *ubsan_type_check_kinds[] = {
    "load of",
    "store to",
    "reference binding to",
    "member access within",
    "member call on",
    "constructor call on",
    "downcast of",
    "downcast of",
    "upcast of",
    "cast to virtual base of",
};

static void ubsan_vlogf(struct ubsan_source_location *location, const char* func, const char *fmt, va_list ap) {
    klog_inl(ERROR, "[%s] in %s:%i:%i: ", func, location->file, location->line, location->column);
    vklog(ERROR, fmt, ap);
}

static void __ubsan_logf(struct ubsan_source_location *location, const char* func, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    ubsan_vlogf(location, func, fmt, ap);
    va_end(ap);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

void __ubsan_handle_add_overflow(struct ubsan_overflow_data *data, unsigned long ul_lhs, unsigned long ul_rhs) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_add_overflow_abort(struct ubsan_overflow_data *data, unsigned long ul_lhs, unsigned long ul_rhs) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_alignment_assumption(struct ubsan_alignment_assumption_data *data, unsigned long ul_pointer, unsigned long ul_alignment, unsigned long ul_offset) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_alignment_assumption_abort(struct ubsan_alignment_assumption_data *data, unsigned long ul_pointer, unsigned long ul_alignment, unsigned long ul_offset) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_builtin_unreachable(struct ubsan_unreachable_data *data) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_cfi_bad_type(struct ubsan_cfi_check_fail_data *data, unsigned long ul_vtable, bool b_valid_vtable, bool from_unrecoverable_handler, unsigned long program_counter, unsigned long frame_pointer) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_cfi_check_fail(struct ubsan_cfi_check_fail_data *data, unsigned long ul_value, unsigned long ul_valid_vtable) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_cfi_check_fail_abort(struct ubsan_cfi_check_fail_data *data, unsigned long ul_value, unsigned long ul_valid_vtable) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_divrem_overflow(struct ubsan_overflow_data *data, unsigned long ul_lhs, unsigned long ul_rhs) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_divrem_overflow_abort(struct ubsan_overflow_data *data, unsigned long ul_lhs, unsigned long ul_rhs) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_dynamic_type_cache_miss(struct ubsan_dynamic_type_cache_miss_data *data, unsigned long ul_pointer, unsigned long ulHash) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_dynamic_type_cache_miss_abort(struct ubsan_dynamic_type_cache_miss_data *data, unsigned long ul_pointer, unsigned long ulHash) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_float_cast_overflow(struct ubsan_float_cast_overflow_data *data, unsigned long ul_from) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_float_cast_overflow_abort(struct ubsan_float_cast_overflow_data *data, unsigned long ul_from) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_function_type_mismatch(struct ubsan_function_type_mismatch_data *data, unsigned long ul_function) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_function_type_mismatch_abort(struct ubsan_function_type_mismatch_data *data, unsigned long ul_function) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_function_type_mismatch_v1(struct ubsan_function_type_mismatch_data *data, unsigned long ul_function, unsigned long ul_callee_rtti, unsigned long ul_fn_rtti) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_function_type_mismatch_v1_abort(struct ubsan_function_type_mismatch_data *data, unsigned long ul_function, unsigned long ul_callee_rtti, unsigned long ul_fn_rtti) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_invalid_builtin(struct ubsan_invalid_builtin_data *data) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_invalid_builtin_abort(struct ubsan_invalid_builtin_data *data) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_load_invalid_value(struct ubsan_invalid_value_data *data, unsigned long ulVal) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_load_invalid_value_abort(struct ubsan_invalid_value_data *data, unsigned long ulVal) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_missing_return(struct ubsan_unreachable_data *data) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_mul_overflow(struct ubsan_overflow_data *data, unsigned long ul_lhs, unsigned long ul_rhs) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_mul_overflow_abort(struct ubsan_overflow_data *data, unsigned long ul_lhs, unsigned long ul_rhs) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_negate_overflow(struct ubsan_overflow_data *data, unsigned long ul_old_val) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_negate_overflow_abort(struct ubsan_overflow_data *data, unsigned long ul_old_val) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_nonnull_arg(struct ubsan_nonnull_arg_data *data) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_nonnull_arg_abort(struct ubsan_nonnull_arg_data *data) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_nonnull_return_v1(struct ubsan_nonnull_return_data *data, struct ubsan_source_location *location_ptr) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_nonnull_return_v1_abort(struct ubsan_nonnull_return_data *data, struct ubsan_source_location *location_ptr) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_nullability_arg(struct ubsan_nonnull_arg_data *data) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_nullability_arg_abort(struct ubsan_nonnull_arg_data *data) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_nullability_return_v1(struct ubsan_nonnull_return_data *data, struct ubsan_source_location *location_ptr) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_nullability_return_v1_abort(struct ubsan_nonnull_return_data *data, struct ubsan_source_location *location_ptr) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_out_of_bounds(struct ubsan_out_of_bounds_data *data, unsigned long ul_index) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_out_of_bounds_abort(struct ubsan_out_of_bounds_data *data, unsigned long ul_index) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_pointer_overflow(struct ubsan_pointer_overflow_data *data, unsigned long ul_base, unsigned long ul_result) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_pointer_overflow_abort(struct ubsan_pointer_overflow_data *data, unsigned long ul_base, unsigned long ul_result) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_shift_out_of_bounds(struct ubsan_shift_out_of_bounds_data *data, unsigned long ul_lhs, unsigned long ul_rhs) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_shift_out_of_bounds_abort(struct ubsan_shift_out_of_bounds_data *data, unsigned long ul_lhs, unsigned long ul_rhs) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_sub_overflow(struct ubsan_overflow_data *data, unsigned long ul_lhs, unsigned long ul_rhs) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_sub_overflow_abort(struct ubsan_overflow_data *data, unsigned long ul_lhs, unsigned long ul_rhs) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_type_mismatch(struct ubsan_type_mismatch_data *data, unsigned long ul_pointer) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_type_mismatch_abort(struct ubsan_type_mismatch_data *data, unsigned long ul_pointer) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_type_mismatch_v1(struct ubsan_type_mismatch_data_v1 *data, unsigned long ul_pointer) {
    if(ul_pointer == 0)
        ubsan_logf(&data->location, "Null pointer access (%s @ %p)", data->type->name, (void*) ul_pointer);
    else if(data->log_alignment != 0 && is_aligned(ul_pointer, data->log_alignment)) {
        ubsan_logf(&data->location, "Unaligned memory access (%s @ %p)", data->type->name, (void*) ul_pointer);
    }
    else 
        ubsan_logf(&data->location, "Type mismatch (%s @ %p)", data->type->name, (void*) ul_pointer);
    dump_stack();
}

void __ubsan_handle_type_mismatch_v1_abort(struct ubsan_type_mismatch_data_v1 *data, unsigned long ul_pointer) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_vla_bound_not_positive(struct ubsan_vla_bound_data *data, unsigned long ul_bound) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_vla_bound_not_positive_abort(struct ubsan_vla_bound_data *data, unsigned long ul_bound) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_implicit_conversion(struct ubsan_implicit_conversion_data *data, unsigned long ul_from, unsigned long ul_to) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

void __ubsan_handle_implicit_conversion_abort(struct ubsan_implicit_conversion_data *data, unsigned long ul_from, unsigned long ul_to) {
    ubsan_logf(&data->location, "triggered %s", __func__);
}

#pragma GCC diagnostic pop

