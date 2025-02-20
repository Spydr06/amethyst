#ifndef _AMETHYST_SANITIZE_UB_H
#define _AMETHYST_SANITIZE_UB_H

#include <stdint.h>

struct ubsan_source_location {
    const char* file;
    uint32_t line;
    uint32_t column;
};

struct ubsan_type_descriptor {
    uint16_t kind;
    uint16_t info;
    char name[1];
};

struct ubsan_overflow_data {
    struct ubsan_source_location location;
    struct ubsan_type_descriptor *type;
};

struct ubsan_alignment_assumption_data {
    struct ubsan_source_location location;
    struct ubsan_source_location assumption_location;
    struct ubsan_type_descriptor *type;
};

struct ubsan_unreachable_data {
    struct ubsan_source_location location;
};

struct ubsan_cfi_check_fail_data {
    uint8_t check_kind;
    struct ubsan_source_location location;
    struct ubsan_type_descriptor *type;
};

struct ubsan_dynamic_type_cache_miss_data {
    struct ubsan_source_location location;
    struct ubsan_type_descriptor *type;
    void *type_info;
    uint8_t type_check_kind;
};

struct ubsan_float_cast_overflow_data {
    struct ubsan_source_location location;
    struct ubsan_type_descriptor *from_type;
    struct ubsan_type_descriptor *to_type;
};

struct ubsan_function_type_mismatch_data {
    struct ubsan_source_location location;
    struct ubsan_type_descriptor *type;
};

struct ubsan_invalid_builtin_data {
    struct ubsan_source_location location;
    struct ubsan_type_descriptor *type;
};

struct ubsan_invalid_value_data {
    struct ubsan_source_location location;
    struct ubsan_type_descriptor *type;
};

struct ubsan_nonnull_arg_data {
    struct ubsan_source_location location;
    struct ubsan_source_location attribute_location;
    int arg_index;
};

struct ubsan_nonnull_value_data {
    struct ubsan_source_location location;
    struct ubsan_type_descriptor *type;
};

struct ubsan_nonnull_return_data {
    struct ubsan_source_location location;
};

struct ubsan_out_of_bounds_data {
    struct ubsan_source_location location;
    struct ubsan_type_descriptor *array_type;
    struct ubsan_type_descriptor *index_type;
};

struct ubsan_pointer_overflow_data {
    struct ubsan_source_location location;
};

struct ubsan_shift_out_of_bounds_data {
    struct ubsan_source_location location;
    struct ubsan_type_descriptor *lhs_type;
    struct ubsan_type_descriptor *rhs_type;
};

struct ubsan_type_mismatch_data {
    struct ubsan_source_location location;
    struct ubsan_type_descriptor *type;
    unsigned long log_alignment;
    uint8_t type_check_kind;
};

struct ubsan_type_mismatch_data_v1 {
    struct ubsan_source_location location;
    struct ubsan_type_descriptor *type;
    uint8_t log_alignment;
    uint8_t type_check_kind;
};

struct ubsan_vla_bound_data {
    struct ubsan_source_location location;
    struct ubsan_type_descriptor *type;
};

struct ubsan_implicit_conversion_data {
    struct ubsan_source_location location;
    struct ubsan_type_descriptor *from_type;
    struct ubsan_type_descriptor *to_type;
    uint8_t kind;
};

extern const char *ubsan_type_check_kinds[];

void __ubsan_handle_add_overflow(struct ubsan_overflow_data *data, unsigned long ul_lhs, unsigned long ul_rhs);
void __ubsan_handle_add_overflow_abort(struct ubsan_overflow_data *data, unsigned long ul_lhs, unsigned long ul_rhs);
void __ubsan_handle_alignment_assumption(struct ubsan_alignment_assumption_data *data, unsigned long ul_pointer, unsigned long ul_alignment, unsigned long ul_offset);
void __ubsan_handle_alignment_assumption_abort(struct ubsan_alignment_assumption_data *data, unsigned long ul_pointer, unsigned long ul_alignment, unsigned long ul_offset);
void __ubsan_handle_builtin_unreachable(struct ubsan_unreachable_data *data);
void __ubsan_handle_cfi_bad_type(struct ubsan_cfi_check_fail_data *data, unsigned long ul_vtable, bool b_valid_vtable, bool from_unrecoverable_handler, unsigned long program_counter, unsigned long frame_pointer);
void __ubsan_handle_cfi_check_fail(struct ubsan_cfi_check_fail_data *data, unsigned long ul_value, unsigned long ul_valid_vtable);
void __ubsan_handle_cfi_check_fail_abort(struct ubsan_cfi_check_fail_data *data, unsigned long ul_value, unsigned long ul_valid_vtable);
void __ubsan_handle_divrem_overflow(struct ubsan_overflow_data *data, unsigned long ul_lhs, unsigned long ul_rhs);
void __ubsan_handle_divrem_overflow_abort(struct ubsan_overflow_data *data, unsigned long ul_lhs, unsigned long ul_rhs);
void __ubsan_handle_dynamic_type_cache_miss(struct ubsan_dynamic_type_cache_miss_data *data, unsigned long ul_pointer, unsigned long ulHash);
void __ubsan_handle_dynamic_type_cache_miss_abort(struct ubsan_dynamic_type_cache_miss_data *data, unsigned long ul_pointer, unsigned long ulHash);
void __ubsan_handle_float_cast_overflow(struct ubsan_float_cast_overflow_data *data, unsigned long ul_from);
void __ubsan_handle_float_cast_overflow_abort(struct ubsan_float_cast_overflow_data *data, unsigned long ul_from);
void __ubsan_handle_function_type_mismatch(struct ubsan_function_type_mismatch_data *data, unsigned long ul_function);
void __ubsan_handle_function_type_mismatch_abort(struct ubsan_function_type_mismatch_data *data, unsigned long ul_function);
void __ubsan_handle_function_type_mismatch_v1(struct ubsan_function_type_mismatch_data *data, unsigned long ul_function, unsigned long ul_callee_rtti, unsigned long ul_fn_rtti);
void __ubsan_handle_function_type_mismatch_v1_abort(struct ubsan_function_type_mismatch_data *data, unsigned long ul_function, unsigned long ul_callee_rtti, unsigned long ul_fn_rtti);
void __ubsan_handle_invalid_builtin(struct ubsan_invalid_builtin_data *data);
void __ubsan_handle_invalid_builtin_abort(struct ubsan_invalid_builtin_data *data);
void __ubsan_handle_load_invalid_value(struct ubsan_invalid_value_data *data, unsigned long ulVal);
void __ubsan_handle_load_invalid_value_abort(struct ubsan_invalid_value_data *data, unsigned long ulVal);
void __ubsan_handle_missing_return(struct ubsan_unreachable_data *data);
void __ubsan_handle_mul_overflow(struct ubsan_overflow_data *data, unsigned long ul_lhs, unsigned long ul_rhs);
void __ubsan_handle_mul_overflow_abort(struct ubsan_overflow_data *data, unsigned long ul_lhs, unsigned long ul_rhs);
void __ubsan_handle_negate_overflow(struct ubsan_overflow_data *data, unsigned long ul_old_val);
void __ubsan_handle_negate_overflow_abort(struct ubsan_overflow_data *data, unsigned long ul_old_val);
void __ubsan_handle_nonnull_arg(struct ubsan_nonnull_arg_data *data);
void __ubsan_handle_nonnull_arg_abort(struct ubsan_nonnull_arg_data *data);
void __ubsan_handle_nonnull_return_v1(struct ubsan_nonnull_return_data *data, struct ubsan_source_location *location_ptr);
void __ubsan_handle_nonnull_return_v1_abort(struct ubsan_nonnull_return_data *data, struct ubsan_source_location *location_ptr);
void __ubsan_handle_nullability_arg(struct ubsan_nonnull_arg_data *data);
void __ubsan_handle_nullability_arg_abort(struct ubsan_nonnull_arg_data *data);
void __ubsan_handle_nullability_return_v1(struct ubsan_nonnull_return_data *data, struct ubsan_source_location *location_ptr);
void __ubsan_handle_nullability_return_v1_abort(struct ubsan_nonnull_return_data *data, struct ubsan_source_location *location_ptr);
void __ubsan_handle_out_of_bounds(struct ubsan_out_of_bounds_data *data, unsigned long ul_index);
void __ubsan_handle_out_of_bounds_abort(struct ubsan_out_of_bounds_data *data, unsigned long ul_index);
void __ubsan_handle_pointer_overflow(struct ubsan_pointer_overflow_data *data, unsigned long ul_base, unsigned long ul_result);
void __ubsan_handle_pointer_overflow_abort(struct ubsan_pointer_overflow_data *data, unsigned long ul_base, unsigned long ul_result);
void __ubsan_handle_shift_out_of_bounds(struct ubsan_shift_out_of_bounds_data *data, unsigned long ul_lhs, unsigned long ul_rhs);
void __ubsan_handle_shift_out_of_bounds_abort(struct ubsan_shift_out_of_bounds_data *data, unsigned long ul_lhs, unsigned long ul_rhs);
void __ubsan_handle_sub_overflow(struct ubsan_overflow_data *data, unsigned long ul_lhs, unsigned long ul_rhs);
void __ubsan_handle_sub_overflow_abort(struct ubsan_overflow_data *data, unsigned long ul_lhs, unsigned long ul_rhs);
void __ubsan_handle_type_mismatch(struct ubsan_type_mismatch_data *data, unsigned long ul_pointer);
void __ubsan_handle_type_mismatch_abort(struct ubsan_type_mismatch_data *data, unsigned long ul_pointer);
void __ubsan_handle_type_mismatch_v1(struct ubsan_type_mismatch_data_v1 *data, unsigned long ul_pointer);
void __ubsan_handle_type_mismatch_v1_abort(struct ubsan_type_mismatch_data_v1 *data, unsigned long ul_pointer);
void __ubsan_handle_vla_bound_not_positive(struct ubsan_vla_bound_data *data, unsigned long ul_bound);
void __ubsan_handle_vla_bound_not_positive_abort(struct ubsan_vla_bound_data *data, unsigned long ul_bound);
void __ubsan_handle_implicit_conversion(struct ubsan_implicit_conversion_data *data, unsigned long ul_from, unsigned long ul_to);
void __ubsan_handle_implicit_conversion_abort(struct ubsan_implicit_conversion_data *data, unsigned long ul_from, unsigned long ul_to);
void __ubsan_get_current_report_data(const char **pp_out_issue_kind, const char **pp_out_message, const char **pp_out_filename, uint32_t *p_out_line, uint32_t *p_out_col, char **pp_out_memory_addr);

#endif /* _AMETHYST_SANITIZE_UB_H */

