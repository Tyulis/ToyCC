#include "diagnostic.h"
#include "arch/x86_64/codegen.h"

namespace toycc::arch::x86_64 {
    void CodeGenerator::generate_global_scope(std::shared_ptr<Scope> scope) {
        ScopeFrame frame = in_scope(scope);

        for (std::shared_ptr<Declaration> declaration : scope->locals_list()) {
            if (declaration->type->category == TypeCategory::FUNCTION)
                continue;

            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Global declarations other than functions are not implemented", declaration->location);
        }

        for (std::shared_ptr<Statement> statement : scope->statements) {
            switch (statement->tag) {
                case StatementTag::MARKER:    generate_marker  (statement);  break;
                case StatementTag::FUNCTION:  generate_function(statement);  break;
                default: throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Unsupported statement type in the global scope", statement->location);
            }
        }
    }

    void CodeGenerator::generate_function(std::shared_ptr<Statement> function) {
        std::shared_ptr<Declaration> declaration = function->output->base.declaration();

        // Generate the function symbol
        write_directive(std::format(".globl {}", declaration->name));
        write_directive(std::format(".type {}, @function", declaration->name));
        write_label(declaration->name);

        // Then the actual code
        push_stack_frame();
        for (std::shared_ptr<Declaration> local : function->block->locals_list())
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Function definitions are not implemented", function->location);
        for (std::shared_ptr<Statement> statement : function->block->statements)
            throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Function definitions are not implemented", function->location);
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
