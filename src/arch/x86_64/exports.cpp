#include "arch/x86_64/codegen.h"

namespace toycc::arch::x86_64 {
    CodeGenerator::CodeGenerator(const TranslationUnit& unit) : toycc::arch::CodeGenerator(unit) {}

    void CodeGenerator::operator() (std::ostream& output) {
        this->output = output;
        generate_translation_unit(unit);
    }
}
