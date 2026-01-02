#pragma once

#include "ir/flow.h"
#include "arch/codegen.h"
#include "arch/x86_64/output.h"
#include "arch/x86_64/allocation.h"

namespace toycc::arch::x86_64 {
    using namespace toycc::ir;

    class CodeGenerator : public toycc::arch::CodeGenerator {
        public:
            // -------- Exported methods -> arch/x86_64/exports.cpp
            CodeGenerator(const TranslationUnit& unit);
            virtual void operator() (std::ostream& output) override;

        private:
            // -------- Global constructs -> arch/x86_64/global.cpp
            void generate_translation_unit(CodeOutput& output, const TranslationUnit& unit);
            void generate_procedure(CodeOutput& output, const Procedure& procedure);
            void generate_local_block(StackFrame& frame, std::shared_ptr<LocalBlock> block);

            // -------- Statements -> arch/x86_64/statements.cpp
            void generate_statement(StackFrame& frame, std::shared_ptr<Statement> statement);
            void generate_marker(StackFrame& frame, std::shared_ptr<Statement> marker);
            void generate_return(StackFrame& frame, std::shared_ptr<Statement> statement);
    };
}
