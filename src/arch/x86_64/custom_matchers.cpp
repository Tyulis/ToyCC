#include "config.h"
#include "diagnostic.h"
#include "arch/x86_64/execmodel.h"
#include "arch/x86_64/constraints.hpp"
#include "arch/x86_64/custom_matchers.h"
#include "util/strings.h"

namespace toycc::arch::x86_64 {
    std::optional<TranslationMatch> match_call(const StackFrame& frame, const ir::DependencyGraph&, const GroupMatch& group_match) {
        const ir::Statement& statement = group_match.statements[0]->statement();
        const ir::Operand& function = statement.inputs[0];
        if (function.type()->storage_category() != ir::TypeCategory::FUNCTION)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempting to call an operand that is not a function", function.location);

        StatementMatch statement_match = {.input = {{OperandMatch::OK, {Location::constant}}}, .output = {}, .is_inout = false};

        if (statement.output.has_value()) {
            const ir::Operand& output_operand = statement.output.value();
            statement_match.output = (check_type(output_operand, ir::TypeCategory::INTEGER) | check_type(output_operand, ir::TypeCategory::BOOL))
                                    & check_out_location(frame, output_operand, RETURN_VALUE_LOCATION);
        }

        size_t integer_argument_index = 0;
        size_t float_argument_index = 0;
        for (auto it = statement.inputs.begin() + 1; it != statement.inputs.end(); it++) {
            const ir::Operand operand = *it;
            const ir::TypeCategory operand_type = operand.type()->storage_category();
            if (operand_type == ir::TypeCategory::INTEGER || operand_type == ir::TypeCategory::BOOL) {
                if (integer_argument_index >= INTEGER_REGISTER_ARGUMENTS.size())
                    throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, std::format("Function calls with more than {} integer arguments are not implemented", INTEGER_REGISTER_ARGUMENTS.size()), statement.location);
                statement_match.input.push_back(check_in_location(frame, operand, INTEGER_REGISTER_ARGUMENTS[integer_argument_index++]));
            } else if (operand_type == ir::TypeCategory::FLOAT) {
                if (float_argument_index >= FLOAT_REGISTER_ARGUMENTS.size())
                    throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, std::format("Function calls with more than {} floating-point arguments are not implemented", FLOAT_REGISTER_ARGUMENTS.size()), statement.location);
                statement_match.input.push_back(check_in_location(frame, operand, FLOAT_REGISTER_ARGUMENTS[float_argument_index++]));
            } else throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Non-scalar argument types are not implemented", statement.location);
        }

        TranslationMatch match = {.translation = TranslationTag::CALL_0, .group_match = group_match, .statements = {statement_match}, .allocations = {}};
        if (toycc::config::debug::with_translation_trace)
            std::cerr << indent(dump(match), true, "        ") << "\n";

        if (match.matches() || match.nof_transfers().has_value())
            return match;
        else
            return {};
    }
}
