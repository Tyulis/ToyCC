#include "arch/x86_64/codegen.h"

namespace toycc::arch::x86_64 {
    std::shared_ptr<Scope> CodeGenerator::current_scope() {
        return scope_stack.back();
    }

    ScopeFrame CodeGenerator::in_scope(std::shared_ptr<Scope> scope) {
        return {scope_stack, scope};
    }
}
