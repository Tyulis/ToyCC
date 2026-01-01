#pragma once

#include <optional>

#include "ir/flow.h"
#include "arch/codegen.h"

namespace toycc::arch::x86_64 {
    using namespace toycc::ir;

    class CodeGenerator : public toycc::arch::CodeGenerator {
        public:
            // -------- Exported methods -> arch/x86_64/exports.cpp
            CodeGenerator(const TranslationUnit& unit);
            virtual void operator() (std::ostream& output) override;

        private:
            std::optional<std::reference_wrapper<std::ostream>> output;

            // -------- Global constructs -> arch/x86_64/global.cpp
            void generate_translation_unit(const TranslationUnit& unit);
            void generate_procedure(const Procedure& procedure);

            void push_stack_frame();
            void pop_stack_frame();

            // -------- Statements -> arch/x86_64/statements.cpp
            void generate_marker(std::shared_ptr<Statement> marker);

            // -------- Common code generation utilities -> arch/x86_64/write.cpp
            void write_label(std::string name);
            void write_statement(std::string code);
            void write_directive(std::string code);
    };
}
