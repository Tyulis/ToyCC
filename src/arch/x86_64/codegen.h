#pragma once

#include <optional>

#include "ir/flow.h"
#include "arch/codegen.h"
#include "arch/x86_64/output.h"

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

            // -------- Statements -> arch/x86_64/statements.cpp
            void generate_marker(std::shared_ptr<Statement> marker);
    };
}
