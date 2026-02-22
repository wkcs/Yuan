/// \file DiagnosticIDs.h
/// \brief Diagnostic ID definitions for the Yuan compiler.
///
/// This file defines all diagnostic IDs used throughout the compiler,
/// organized by category (lexer, parser, semantic, warnings, notes).

#ifndef YUAN_BASIC_DIAGNOSTICIDS_H
#define YUAN_BASIC_DIAGNOSTICIDS_H

#include <cstdint>
#include <string>

namespace yuan {

/// \brief Diagnostic severity levels.
enum class DiagnosticLevel {
    Note,       ///< Informational note
    Warning,    ///< Warning (compilation continues)
    Error,      ///< Error (compilation may continue but will fail)
    Fatal       ///< Fatal error (compilation stops immediately)
};

/// \brief Diagnostic message IDs.
///
/// Each diagnostic has a unique ID for documentation and lookup purposes.
/// The ID ranges are organized as follows:
/// - 1xxx: Lexer errors
/// - 2xxx: Parser errors
/// - 3xxx: Semantic errors
/// - 4xxx: Warnings
/// - 5xxx: Notes
enum class DiagID : uint16_t {
    // =========================================================================
    // Lexer errors (1xxx)
    // =========================================================================
    
    /// Invalid character encountered in source
    err_invalid_character = 1001,
    
    /// Unterminated string literal
    err_unterminated_string = 1002,
    
    /// Unterminated character literal
    err_unterminated_char = 1003,
    
    /// Invalid escape sequence in string or character literal
    err_invalid_escape_sequence = 1004,
    
    /// Invalid number literal format
    err_invalid_number_literal = 1005,
    
    /// Unterminated block comment
    err_unterminated_block_comment = 1006,
    
    /// Empty character literal
    err_empty_char_literal = 1007,
    
    /// Character literal too long
    err_char_literal_too_long = 1008,
    
    /// Invalid Unicode escape sequence
    err_invalid_unicode_escape = 1009,
    
    /// Invalid hex digit in escape sequence
    err_invalid_hex_digit = 1010,
    
    /// Number literal overflow
    err_number_overflow = 1011,
    
    /// Invalid digit for number base
    err_invalid_digit_for_base = 1012,
    
    /// Invalid type suffix on number literal
    err_invalid_number_suffix = 1013,
    
    /// Unterminated raw string literal
    err_unterminated_raw_string = 1014,
    
    /// Unterminated multiline string literal
    err_unterminated_multiline_string = 1015,
    
    /// Invalid character literal
    err_invalid_character_literal = 1017,
    
    /// Invalid string literal
    err_invalid_string_literal = 1018,
    
    // =========================================================================
    // Parser errors (2xxx)
    // =========================================================================
    
    /// Expected a specific token
    err_expected_token = 2001,
    
    /// Expected an expression
    err_expected_expression = 2002,
    
    /// Expected a type
    err_expected_type = 2003,
    
    /// Expected an identifier
    err_expected_identifier = 2004,
    
    /// Unexpected token
    err_unexpected_token = 2005,
    
    /// Invalid operator
    err_invalid_operator = 2006,
    
    /// Expected a statement
    err_expected_statement = 2007,
    
    /// Expected a declaration
    err_expected_declaration = 2008,
    
    /// Expected a pattern
    err_expected_pattern = 2009,
    
    /// Expected a block
    err_expected_block = 2010,
    
    /// Expected a function body
    err_expected_function_body = 2011,
    
    /// Expected a parameter list
    err_expected_param_list = 2012,
    
    /// Expected a struct field
    err_expected_struct_field = 2013,
    
    /// Expected an enum variant
    err_expected_enum_variant = 2014,
    
    /// Expected a match arm
    err_expected_match_arm = 2015,
    
    /// Invalid visibility modifier
    err_invalid_visibility = 2016,
    
    /// Duplicate modifier
    err_duplicate_modifier = 2017,
    
    /// Expected semicolon
    err_expected_semicolon = 2018,
    
    /// Expected comma or closing delimiter
    err_expected_comma_or_close = 2019,
    
    /// Expected left brace
    err_expected_lbrace = 2020,
    
    /// Expected right brace
    err_expected_rbrace = 2021,
    
    /// Expected colon
    err_expected_colon = 2022,
    
    /// Expected arrow (-> or =>)
    err_expected_arrow = 2023,
    
    /// Expected fat arrow (=>)
    err_expected_fat_arrow = 2024,
    
    /// Invalid array size
    err_invalid_array_size = 2025,
    
    /// Expected generic parameter
    err_expected_generic_param = 2026,
    
    /// Expected trait bound
    err_expected_trait_bound = 2027,
    
    /// Expected 'in' keyword in for loop
    err_expected_in = 2028,
    
    /// Expected builtin identifier
    err_expected_builtin_identifier = 2029,
    
    /// Invalid builtin function name
    err_invalid_builtin_name = 2030,
    
    /// Expected pipe or func keyword for closure
    err_expected_pipe_or_func = 2031,
    
    /// Unknown builtin function
    err_unknown_builtin_function = 2032,
    
    /// Expression statement has no effect
    err_expression_statement_no_effect = 2033,

    /// Variadic parameter must be last
    err_variadic_param_must_be_last = 2034,

    // =========================================================================
    // Semantic errors (3xxx)
    // =========================================================================
    
    /// Undeclared identifier
    err_undeclared_identifier = 3001,
    
    /// Redefinition of symbol
    err_redefinition = 3002,
    
    /// Type mismatch
    err_type_mismatch = 3003,
    
    /// Cannot assign to const variable
    err_cannot_assign_to_const = 3004,
    
    /// Invalid operand types for operator
    err_invalid_operand_types = 3005,
    
    /// Function not found
    err_function_not_found = 3006,
    
    /// Wrong number of arguments
    err_wrong_argument_count = 3007,
    
    /// Return type mismatch
    err_return_type_mismatch = 3008,
    
    /// Missing return statement
    err_missing_return = 3009,
    
    /// Unused result (non-void return value not used)
    err_unused_result = 3010,
    
    /// Cannot assign to immutable variable
    err_cannot_assign_to_immutable = 3011,
    
    /// Cannot take mutable reference to immutable value
    err_cannot_mut_ref_immutable = 3012,
    
    /// Break outside of loop
    err_break_outside_loop = 3013,
    
    /// Continue outside of loop
    err_continue_outside_loop = 3014,
    
    /// Return outside of function
    err_return_outside_function = 3015,
    
    /// Invalid cast
    err_invalid_cast = 3016,
    
    /// Cannot infer type
    err_cannot_infer_type = 3017,
    
    /// Recursive type definition
    err_recursive_type = 3018,
    
    /// Trait not implemented
    err_trait_not_implemented = 3019,
    
    /// Method not found
    err_method_not_found = 3020,
    
    /// Field not found
    err_field_not_found = 3021,
    
    /// Private member access
    err_private_member_access = 3022,
    
    /// Non-exhaustive match
    err_non_exhaustive_match = 3023,
    
    /// Duplicate match arm
    err_duplicate_match_arm = 3024,
    
    /// Invalid pattern for type
    err_invalid_pattern_for_type = 3025,
    
    /// Error propagation in non-error function
    err_error_propagation_invalid = 3026,
    
    /// Error type not implemented
    err_error_type_not_implemented = 3027,
    
    /// Circular import
    err_circular_import = 3028,
    
    /// Module not found
    err_module_not_found = 3029,
    
    /// Ambiguous reference
    err_ambiguous_reference = 3030,
    
    /// Invalid main function signature
    err_invalid_main_signature = 3031,
    
    /// Duplicate trait implementation
    err_duplicate_trait_impl = 3032,
    
    /// Missing trait method
    err_missing_trait_method = 3033,
    
    /// Trait method signature mismatch
    err_trait_method_signature_mismatch = 3034,
    
    /// Generic parameter count mismatch
    err_generic_param_count_mismatch = 3035,
    
    /// Constraint not satisfied
    err_constraint_not_satisfied = 3036,

    /// Default trait method not supported
    err_default_trait_method_not_supported = 3041,

    /// Enum variant must be qualified with its enum type
    err_unqualified_enum_variant = 3042,

    /// Type name used as value in var/const binding
    err_type_used_as_value = 3043,

    /// Wrong number of arguments for builtin calls
    err_wrong_builtin_argument_count = 3044,

    /// Invalid borrow from non-borrowable value
    err_invalid_borrow = 3045,

    /// Unhandled error propagation in non-error function
    err_unhandled_error_propagation = 3046,

    /// await outside async function
    err_await_outside_async = 3047,

    /// Operator traits cannot be implemented for builtin types
    err_builtin_operator_overload_forbidden = 3048,

    /// Cannot dereference non-pointer type
    err_cannot_deref_non_pointer = 3037,
    
    /// Cannot index non-array type
    err_cannot_index_non_array = 3038,
    
    /// Index out of bounds (compile-time)
    err_index_out_of_bounds = 3039,
    
    /// Division by zero (compile-time)
    err_division_by_zero = 3040,
    
    // =========================================================================
    // Warnings (4xxx)
    // =========================================================================
    
    /// Unused variable
    warn_unused_variable = 4001,
    
    /// Unreachable code
    warn_unreachable_code = 4002,
    
    /// Implicit conversion
    warn_implicit_conversion = 4003,
    
    /// Unused import
    warn_unused_import = 4004,
    
    /// Unused function
    warn_unused_function = 4005,
    
    /// Unused parameter
    warn_unused_parameter = 4006,
    
    /// Shadowed variable
    warn_shadowed_variable = 4007,
    
    /// Deprecated feature
    warn_deprecated = 4008,
    
    /// Comparison always true/false
    warn_comparison_always = 4009,
    
    /// Integer overflow in constant expression
    warn_integer_overflow = 4010,
    
    /// Unnecessary cast
    warn_unnecessary_cast = 4011,
    
    /// Empty match arm
    warn_empty_match_arm = 4012,
    
    /// Unused match arm
    warn_unused_match_arm = 4013,
    
    /// Missing else branch
    warn_missing_else = 4014,
    
    /// Unused field
    warn_unused_field = 4015,

    /// Unused result
    warn_unused_result = 4016,

    /// Enum variant name conflicts with function; function is preferred
    warn_enum_variant_function_preferred = 4017,

    // =========================================================================
    // Notes (5xxx)
    // =========================================================================
    
    /// Note: declared here
    note_declared_here = 5001,
    
    /// Note: previous definition here
    note_previous_definition = 5002,
    
    /// Note: candidate function
    note_candidate_function = 5003,
    
    /// Note: required by trait
    note_required_by_trait = 5004,
    
    /// Note: in instantiation of
    note_in_instantiation = 5005,
    
    /// Note: while checking
    note_while_checking = 5006,
    
    /// Note: did you mean
    note_did_you_mean = 5007,
    
    /// Note: imported here
    note_imported_here = 5008,
    
    /// Note: type is
    note_type_is = 5009,
    
    /// Note: expected type
    note_expected_type = 5010,
    
    /// Note: found type
    note_found_type = 5011,
    
    /// Note: consider adding
    note_consider_adding = 5012,
    
    /// Note: defined in trait
    note_defined_in_trait = 5013,
};

/// \brief Get the diagnostic level for a given diagnostic ID.
/// \param id The diagnostic ID.
/// \return The diagnostic level.
DiagnosticLevel getDiagnosticLevel(DiagID id);

/// \brief Get the format string for a diagnostic ID.
/// \param id The diagnostic ID.
/// \return The format string with placeholders like {0}, {1}, etc.
const char* getDiagnosticFormatString(DiagID id);

/// \brief Get the error code string for a diagnostic ID.
/// \param id The diagnostic ID.
/// \return The error code string (e.g., "E1001").
std::string getDiagnosticCode(DiagID id);

/// \brief Check if a diagnostic ID represents an error.
/// \param id The diagnostic ID.
/// \return True if the diagnostic is an error or fatal error.
inline bool isError(DiagID id) {
    auto level = getDiagnosticLevel(id);
    return level == DiagnosticLevel::Error || level == DiagnosticLevel::Fatal;
}

/// \brief Check if a diagnostic ID represents a warning.
/// \param id The diagnostic ID.
/// \return True if the diagnostic is a warning.
inline bool isWarning(DiagID id) {
    return getDiagnosticLevel(id) == DiagnosticLevel::Warning;
}

/// \brief Check if a diagnostic ID represents a note.
/// \param id The diagnostic ID.
/// \return True if the diagnostic is a note.
inline bool isNote(DiagID id) {
    return getDiagnosticLevel(id) == DiagnosticLevel::Note;
}

} // namespace yuan

#endif // YUAN_BASIC_DIAGNOSTICIDS_H
