#include "arch/x86_64/codegen.h"

namespace toycc::arch::x86_64 {
    CodeGenerator::CodeGenerator(std::shared_ptr<ir::Scope> global_scope) : toycc::arch::CodeGenerator(global_scope) {}

    void CodeGenerator::operator() (std::ostream& output) {
        this->output = output;
        generate_global_scope(global_scope);
    }
}
