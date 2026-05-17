#include "arch/codegen.h"

namespace toycc::arch {
    CodeGenerator::CodeGenerator(const flow::TranslationUnit& unit) : unit(unit) {}
}
