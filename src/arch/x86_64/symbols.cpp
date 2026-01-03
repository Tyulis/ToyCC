#include "arch/x86_64/codegen.h"

namespace toycc::arch::x86_64 {
    std::string CodeGenerator::anonymous_identifier() {
        return std::format(".X{}", unique_id++);
    }
}
