#include "diagnostic.h"
#include "arch/x86_64/codegen.h"

namespace toycc::arch::x86_64 {
    static const std::unordered_map<size_t, std::string> MOV_MNEMONIC = {{1, "movb"}, {2, "movw"}, {4, "movl"}, {8, "movq"}};

    void CodeGenerator::load_constant(StackFrame& frame, const Constant& constant, LOC destination, CodeLocation code_location) {
        if (!constant.is_integer() || constant.type->category != TypeCategory::INTEGER)
            Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Only integer constant loads are implemented", code_location);

        const size_t size = constant.type->size(code_location);
        auto mnemonic = MOV_MNEMONIC.find(size);
        if (mnemonic == MOV_MNEMONIC.end())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("No `mov` instruction for size {}", size), code_location);

        std::optional<std::string> destination_ref = register_ref(destination, size);
        if (!destination_ref.has_value())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Loading a constant to something other than a register", code_location);

        std::shared_ptr<Declaration> constant_decl = std::make_shared<Declaration>(anonymous_identifier(), constant.type, code_location, StorageClass::AUTO | StorageClass::TEMPORARY);

        std::stringstream code;
        code << mnemonic->second << " $" << constant.integer() << ", " << destination_ref.value();
        frame.output.statement(code.str());
        frame.move(constant_decl, destination);
    }

    void CodeGenerator::move_variable(StackFrame& frame, std::shared_ptr<Declaration> variable, LOC destination, CodeLocation code_location) {
        const size_t size = variable->type->size(code_location);
        auto mnemonic = MOV_MNEMONIC.find(size);
        if (mnemonic == MOV_MNEMONIC.end())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("No `mov` instruction for size {}", size), code_location);

        std::string source_ref = variable_ref(frame, variable, code_location);
        std::string destination_ref = variable_ref(frame, variable, destination, code_location);

        std::stringstream code;
        code << mnemonic->second << " " << source_ref << ", " << destination_ref;
        frame.output.statement(code.str());
        frame.move(variable, destination);
    }
}
