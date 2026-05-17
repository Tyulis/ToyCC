#include "postprocessing/postprocessor.h"

namespace toycc::ir {
    void PostProcessor::extract_strings(std::shared_ptr<Scope> global_scope) {
        std::unordered_map<std::shared_ptr<Declaration>, std::string> literals = extract_string_literals(global_scope);

        for (const auto& [declaration, value] : literals) {
            Constant constant = {value, declaration->location, declaration->type};
            global_scope->add_local(declaration);
            global_scope->statements.insert(global_scope->statements.begin(),
                                            Statement::make_unary_operation(declaration->location, StatementTag::COPY, constant, declaration));
        }
    }

    std::unordered_map<std::shared_ptr<Declaration>, std::string> PostProcessor::extract_string_literals(std::shared_ptr<Scope> scope) {
        std::unordered_map<std::shared_ptr<Declaration>, std::string> literals;
        for (Statement& statement : scope->statements) {
            if (statement.block.get() != nullptr)
                literals.insert_range(extract_string_literals(statement.block));

            for (Operand& input : statement.inputs)
                extract_string_literals(literals, input);

            if (statement.output.has_value())
                extract_string_literals(literals, statement.output.value());
        }

        return literals;
    }


    void PostProcessor::extract_string_literals(std::unordered_map<std::shared_ptr<Declaration>, std::string>& literals, Operand& operand) {
        if (!operand.has_constant_base() || operand.constant().tag() != Constant::STRING)
            return;

        std::string value = operand.constant().string();
        std::shared_ptr<Declaration> global = std::make_shared<Declaration>(anonymous_identifier(), operand.constant().type, operand.location,
                                                                            StorageClass::STATIC | StorageClass::TEMPORARY | StorageClass::GLOBAL);

        literals[global] = value;
        operand.value = global;
    }
}
