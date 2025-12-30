#include "diagnostic.h"
#include "arch/x86_64/codegen.h"

namespace toycc::arch::x86_64 {
    CodeGenerator::CodeGenerator(std::shared_ptr<ir::Scope> global_scope) : toycc::arch::CodeGenerator(global_scope) {}

    void CodeGenerator::operator() (std::ostream& output) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Code generation is not implemented");
    }
}
