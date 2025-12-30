#pragma once

#include <optional>

#include "arch/codegen.h"
#include "ir/scope.h"
#include "ir/scopeframe.h"

namespace toycc::arch::x86_64 {
    using namespace toycc::ir;

    class CodeGenerator : public toycc::arch::CodeGenerator {
        public:
            // -------- Exported methods -> arch/x86_64/exports.cpp
            CodeGenerator(std::shared_ptr<Scope> scope);
            virtual void operator() (std::ostream& output) override;

        private:
            std::optional<std::reference_wrapper<std::ostream>> output;
            ScopeStack scope_stack;

            // -------- Global constructs -> arch/x86_64/global.cpp
            void generate_global_scope(std::shared_ptr<Scope> scope);
            void generate_function(std::shared_ptr<stmt::Function> function);

            void push_stack_frame();
            void pop_stack_frame();

            // -------- Statements -> arch/x86_64/statements.cpp
            void generate_marker(std::shared_ptr<stmt::Marker> marker);

            // -------- Common code generation utilities -> arch/x86_64/write.cpp
            void write_label(std::string name);
            void write_statement(std::string code);
            void write_directive(std::string code);

            // -------- State management utilities -> arch/x86_64/state.cpp
            std::shared_ptr<Scope> current_scope();
            ScopeFrame in_scope(std::shared_ptr<Scope> scope);
    };
}
