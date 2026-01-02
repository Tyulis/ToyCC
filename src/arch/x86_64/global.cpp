#include "diagnostic.h"
#include "arch/x86_64/codegen.h"

namespace toycc::arch::x86_64 {
    void CodeGenerator::generate_translation_unit(CodeOutput& output, const TranslationUnit& unit) {
        for (const auto& [name, declaration] : unit.globals) {
            if (declaration->type->category == TypeCategory::FUNCTION)
                continue;

            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Global declarations other than functions are not implemented", declaration->location);
        }

        for (const auto& [name, procedure] : unit.procedures)
            generate_procedure(output, procedure);
    }

    void CodeGenerator::generate_procedure(CodeOutput& output, const Procedure& procedure) {
        // Generate the function symbol
        output.directive(std::format(".globl {}", procedure.declaration->name));
        output.directive(std::format(".type {}, @function", procedure.declaration->name));
        output.label(procedure.declaration->name);

        // Then the actual code
        StackFrame frame;
        //throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Code generation of procedures is not implemented", procedure.declaration->location);
        output << frame;
    }
}
