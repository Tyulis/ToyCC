#include "diagnostic.h"
#include "arch/x86_64/codegen.h"

namespace toycc::arch::x86_64 {
    void CodeGenerator::generate_translation_unit(const TranslationUnit& unit) {
        for (const auto& [name, declaration] : unit.globals) {
            if (declaration->type->category == TypeCategory::FUNCTION)
                continue;

            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Global declarations other than functions are not implemented", declaration->location);
        }

        for (const auto& [name, procedure] : unit.procedures)
            generate_procedure(procedure);
    }

    void CodeGenerator::generate_procedure(const Procedure& procedure) {
        // Generate the function symbol
        write_directive(std::format(".globl {}", procedure.declaration->name));
        write_directive(std::format(".type {}, @function", procedure.declaration->name));
        write_label(procedure.declaration->name);

        // Then the actual code
        push_stack_frame();
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Code generation of procedures is not implemented", procedure.declaration->location);
        pop_stack_frame();
    }

    void CodeGenerator::push_stack_frame() {
        write_statement("pushq %rbp");
        write_statement("movq %rsp, %rbp");
    }

    void CodeGenerator::pop_stack_frame() {
        write_statement("popq %rbp");
        write_statement("ret");
    }

}
