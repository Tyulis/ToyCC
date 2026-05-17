#include "arch/x86_64/codegen.h"

namespace toycc::arch::x86_64 {
    CodeGenerator::CodeGenerator(const flow::TranslationUnit& unit) : toycc::arch::CodeGenerator(unit) {}

    void CodeGenerator::operator() (std::ostream& output) {
        CodeOutput code;
        generate_translation_unit(code, unit);
        output << code;
    }
}
