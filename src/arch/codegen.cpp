#include "arch/codegen.h"

namespace toycc::arch {
    CodeGenerator::CodeGenerator(const ir::TranslationUnit& unit) : unit(unit) {}
}
