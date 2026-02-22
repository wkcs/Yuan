/// \file DiagnosticIDs.cpp
/// \brief Implementation of diagnostic ID utilities.

#include "yuan/Basic/DiagnosticIDs.h"
#include <sstream>
#include <iomanip>

namespace yuan {

DiagnosticLevel getDiagnosticLevel(DiagID id) {
    uint16_t code = static_cast<uint16_t>(id);
    
    if (code >= 1000 && code < 4000) {
        return DiagnosticLevel::Error;
    } else if (code >= 4000 && code < 5000) {
        return DiagnosticLevel::Warning;
    } else if (code >= 5000 && code < 6000) {
        return DiagnosticLevel::Note;
    }
    
    return DiagnosticLevel::Error;
}

const char* getDiagnosticFormatString(DiagID id) {
    switch (id) {
        // Lexer errors
        case DiagID::err_invalid_character:
            return "invalid character '{0}'";
        case DiagID::err_unterminated_string:
            return "unterminated string literal";
        case DiagID::err_unterminated_char:
            return "unterminated character literal";
        case DiagID::err_invalid_escape_sequence:
            return "invalid escape sequence '\\{0}'";
        case DiagID::err_invalid_number_literal:
            return "invalid number literal";
        case DiagID::err_unterminated_block_comment:
            return "unterminated block comment";
        case DiagID::err_empty_char_literal:
            return "empty character literal";
        case DiagID::err_char_literal_too_long:
            return "character literal may only contain one codepoint";
        case DiagID::err_invalid_unicode_escape:
            return "invalid Unicode escape sequence";
        case DiagID::err_invalid_hex_digit:
            return "invalid hex digit '{0}' in escape sequence";
        case DiagID::err_number_overflow:
            return "number literal overflow";
        case DiagID::err_invalid_digit_for_base:
            return "invalid digit '{0}' for base {1} literal";
        case DiagID::err_invalid_number_suffix:
            return "invalid suffix '{0}' on number literal";
        case DiagID::err_unterminated_raw_string:
            return "unterminated raw string literal";
        case DiagID::err_unterminated_multiline_string:
            return "unterminated multiline string literal";
            
        // Parser errors
        case DiagID::err_expected_token:
            return "expected '{0}', found '{1}'";
        case DiagID::err_expected_expression:
            return "expected expression";
        case DiagID::err_expected_type:
            return "expected parameter type";
        case DiagID::err_expected_identifier:
            return "expected identifier";
        case DiagID::err_unexpected_token:
            return "unexpected token '{0}'";
        case DiagID::err_invalid_operator:
            return "invalid operator '{0}'";
        case DiagID::err_expected_statement:
            return "expected statement";
        case DiagID::err_expected_declaration:
            return "expected declaration";
        case DiagID::err_expected_pattern:
            return "expected pattern";
        case DiagID::err_expected_block:
            return "expected '{{' to begin block";
        case DiagID::err_expected_function_body:
            return "expected function body";
        case DiagID::err_expected_param_list:
            return "expected parameter list";
        case DiagID::err_expected_struct_field:
            return "expected struct field";
        case DiagID::err_expected_enum_variant:
            return "expected enum variant";
        case DiagID::err_expected_match_arm:
            return "expected match arm";
        case DiagID::err_invalid_visibility:
            return "invalid visibility modifier '{0}'";
        case DiagID::err_duplicate_modifier:
            return "duplicate modifier '{0}'";
        case DiagID::err_expected_semicolon:
            return "expected ';'";
        case DiagID::err_expected_comma_or_close:
            return "expected ',' or '{0}'";
        case DiagID::err_expected_lbrace:
            return "expected '{{'";
        case DiagID::err_expected_rbrace:
            return "expected '}}'";
        case DiagID::err_expected_colon:
            return "expected ':'";
        case DiagID::err_expected_arrow:
            return "expected '->' or '=>'";
        case DiagID::err_expected_fat_arrow:
            return "expected '=>'";
        case DiagID::err_invalid_array_size:
            return "invalid array size";
        case DiagID::err_expected_generic_param:
            return "expected generic parameter";
        case DiagID::err_expected_trait_bound:
            return "expected trait bound";
        case DiagID::err_expected_in:
            return "expected 'in' in for loop";
        case DiagID::err_expected_builtin_identifier:
            return "expected builtin identifier";
        case DiagID::err_invalid_builtin_name:
            return "invalid builtin function name '{0}'";
        case DiagID::err_expected_pipe_or_func:
            return "expected '|' or 'func' for closure expression";
        case DiagID::err_unknown_builtin_function:
            return "unknown builtin function '{0}'";
        case DiagID::err_expression_statement_no_effect:
            return "expression statement has no effect";
        case DiagID::err_variadic_param_must_be_last:
            return "variadic parameter must be the last parameter";

        // Semantic errors
        case DiagID::err_undeclared_identifier:
            return "use of undeclared identifier '{0}'";
        case DiagID::err_redefinition:
            return "symbol '{0}' redeclared";
        case DiagID::err_type_mismatch:
            return "type mismatch: expected '{0}', found '{1}'";
        case DiagID::err_cannot_assign_to_const:
            return "cannot assign to immutable variable '{0}'";
        case DiagID::err_invalid_operand_types:
            return "type mismatch: invalid operand types for operator '{0}': '{1}' and '{2}'";
        case DiagID::err_function_not_found:
            return "function '{0}' not found";
        case DiagID::err_wrong_argument_count:
            return "wrong number of arguments: expected {0}, found {1}";
        case DiagID::err_wrong_builtin_argument_count:
            return "wrong number of arguments: expected {0}, found {1}";
        case DiagID::err_invalid_borrow:
            return "invalid borrow of non-borrowable value '{0}'";
        case DiagID::err_unhandled_error_propagation:
            return "unhandled error propagation in function '{0}'; handle with '! -> err { ... }' or mark function as error-returning";
        case DiagID::err_await_outside_async:
            return "'await' can only be used inside async functions";
        case DiagID::err_builtin_operator_overload_forbidden:
            return "builtin type '{0}' cannot implement operator trait '{1}'";
        case DiagID::err_return_type_mismatch:
            return "return type mismatch: expected '{0}', found '{1}'";
        case DiagID::err_missing_return:
            return "missing return statement in function returning '{0}'";
        case DiagID::err_unused_result:
            return "unused result of function returning '{0}'";
        case DiagID::err_cannot_assign_to_immutable:
            return "cannot assign to immutable variable '{0}'";
        case DiagID::err_cannot_mut_ref_immutable:
            return "cannot take mutable reference to immutable value";
        case DiagID::err_break_outside_loop:
            return "'break' outside of loop";
        case DiagID::err_continue_outside_loop:
            return "'continue' outside of loop";
        case DiagID::err_return_outside_function:
            return "'return' outside of function";
        case DiagID::err_invalid_cast:
            return "invalid cast: cannot cast '{0}' to '{1}'";
        case DiagID::err_cannot_infer_type:
            return "cannot infer type for '{0}'";
        case DiagID::err_recursive_type:
            return "recursive type '{0}' has infinite size";
        case DiagID::err_trait_not_implemented:
            return "trait '{0}' is not implemented for type '{1}'";
        case DiagID::err_method_not_found:
            return "method '{0}' not found for type '{1}'";
        case DiagID::err_field_not_found:
            return "field or method '{0}' not found in type '{1}'";
        case DiagID::err_private_member_access:
            return "'{0}' is private";
        case DiagID::err_non_exhaustive_match:
            return "non-exhaustive match: pattern '{0}' not covered";
        case DiagID::err_duplicate_match_arm:
            return "duplicate match arm for pattern '{0}'";
        case DiagID::err_invalid_pattern_for_type:
            return "invalid pattern for type '{0}'";
        case DiagID::err_error_propagation_invalid:
            return "'!' can only be used on expressions that may return errors";
        case DiagID::err_error_type_not_implemented:
            return "type '{0}' does not implement Error trait";
        case DiagID::err_circular_import:
            return "circular import detected: {0}";
        case DiagID::err_module_not_found:
            return "module '{0}' not found";
        case DiagID::err_ambiguous_reference:
            return "ambiguous reference to '{0}'";
        case DiagID::err_invalid_main_signature:
            return "invalid main function signature";
        case DiagID::err_duplicate_trait_impl:
            return "duplicate implementation of trait '{0}' for type '{1}'";
        case DiagID::err_missing_trait_method:
            return "missing implementation of trait method '{0}'";
        case DiagID::err_trait_method_signature_mismatch:
            return "method '{0}' has incompatible signature with trait";
        case DiagID::err_generic_param_count_mismatch:
            return "wrong number of generic parameters: expected {0}, found {1}";
        case DiagID::err_constraint_not_satisfied:
            return "constraint '{0}' is not satisfied for type '{1}'";
        case DiagID::err_default_trait_method_not_supported:
            return "default trait method '{0}' is not supported yet";
        case DiagID::err_unqualified_enum_variant:
            return "enum variant '{0}' must be qualified as '{1}.{0}'";
        case DiagID::err_type_used_as_value:
            return "cannot bind type to value '{0}'; use 'type {0} = ...' instead";
        case DiagID::err_cannot_deref_non_pointer:
            return "cannot dereference non-pointer type '{0}'";
        case DiagID::err_cannot_index_non_array:
            return "cannot index non-array type '{0}'";
        case DiagID::err_index_out_of_bounds:
            return "index {0} out of bounds for array of size {1}";
        case DiagID::err_division_by_zero:
            return "division by zero";
            
        // Warnings
        case DiagID::warn_unused_variable:
            return "unused variable '{0}'";
        case DiagID::warn_unreachable_code:
            return "unreachable code";
        case DiagID::warn_implicit_conversion:
            return "implicit conversion from '{0}' to '{1}'";
        case DiagID::warn_unused_import:
            return "unused import '{0}'";
        case DiagID::warn_unused_function:
            return "unused function '{0}'";
        case DiagID::warn_unused_parameter:
            return "unused parameter '{0}'";
        case DiagID::warn_shadowed_variable:
            return "variable '{0}' shadows previous declaration";
        case DiagID::warn_deprecated:
            return "'{0}' is deprecated";
        case DiagID::warn_comparison_always:
            return "comparison is always {0}";
        case DiagID::warn_integer_overflow:
            return "integer overflow in constant expression";
        case DiagID::warn_unnecessary_cast:
            return "unnecessary cast from '{0}' to '{1}'";
        case DiagID::warn_empty_match_arm:
            return "empty match arm";
        case DiagID::warn_unused_match_arm:
            return "unused match arm";
        case DiagID::warn_missing_else:
            return "if expression without else branch";
        case DiagID::warn_unused_field:
            return "unused field '{0}'";
        case DiagID::warn_unused_result:
            return "unused result of function call";
        case DiagID::warn_enum_variant_function_preferred:
            return "enum variant '{0}' is ambiguous; using function '{0}' for enum '{1}'";

        // Notes
        case DiagID::note_declared_here:
            return "'{0}' declared here";
        case DiagID::note_previous_definition:
            return "previous definition of '{0}' is here";
        case DiagID::note_candidate_function:
            return "candidate function: {0}";
        case DiagID::note_required_by_trait:
            return "required by trait '{0}'";
        case DiagID::note_in_instantiation:
            return "in instantiation of '{0}'";
        case DiagID::note_while_checking:
            return "while checking '{0}'";
        case DiagID::note_did_you_mean:
            return "did you mean '{0}'?";
        case DiagID::note_imported_here:
            return "imported here";
        case DiagID::note_type_is:
            return "type is '{0}'";
        case DiagID::note_expected_type:
            return "expected type '{0}'";
        case DiagID::note_found_type:
            return "found type '{0}'";
        case DiagID::note_consider_adding:
            return "consider adding '{0}'";
        case DiagID::note_defined_in_trait:
            return "defined in trait '{0}'";
            
        default:
            return "unknown diagnostic";
    }
}

std::string getDiagnosticCode(DiagID id) {
    uint16_t code = static_cast<uint16_t>(id);
    switch (id) {
        case DiagID::err_expected_token:
            code = 2001;
            break;
        case DiagID::err_variadic_param_must_be_last:
            code = 2003;
            break;
        case DiagID::err_redefinition:
            code = 3020;
            break;
        case DiagID::err_invalid_operand_types:
            code = 3003;
            break;
        case DiagID::err_wrong_argument_count:
            code = 3015;
            break;
        case DiagID::err_wrong_builtin_argument_count:
            code = 3006;
            break;
        case DiagID::err_missing_return:
            code = 3016;
            break;
        case DiagID::err_return_type_mismatch:
            code = 3003;
            break;
        case DiagID::err_cannot_assign_to_const:
        case DiagID::err_cannot_assign_to_immutable:
        case DiagID::err_cannot_mut_ref_immutable:
            code = 3012;
            break;
        case DiagID::err_invalid_cast:
            code = 3037;
            break;
        case DiagID::err_error_propagation_invalid:
            code = 3027;
            break;
        case DiagID::err_error_type_not_implemented:
            code = 3017;
            break;
        case DiagID::err_missing_trait_method:
            code = 3034;
            break;
        case DiagID::err_invalid_borrow:
            code = 3038;
            break;
        case DiagID::err_non_exhaustive_match:
            code = 2014;
            break;
        case DiagID::err_expected_pattern:
            code = 2008;
            break;
        default:
            break;
    }
    std::ostringstream oss;
    
    // Determine prefix based on diagnostic level
    DiagnosticLevel level = getDiagnosticLevel(id);
    switch (level) {
        case DiagnosticLevel::Error:
        case DiagnosticLevel::Fatal:
            oss << "E";
            break;
        case DiagnosticLevel::Warning:
            oss << "W";
            break;
        case DiagnosticLevel::Note:
            oss << "N";
            break;
    }
    
    oss << std::setfill('0') << std::setw(4) << code;
    return oss.str();
}

} // namespace yuan
