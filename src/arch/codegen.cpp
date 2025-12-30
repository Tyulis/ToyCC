#include "arch/codegen.h"

namespace toycc::arch {
    CodeGenerator::CodeGenerator(std::shared_ptr<ir::Scope> global_scope) : global_scope(global_scope) {}
}
