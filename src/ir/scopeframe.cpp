#include "ir/scopeframe.h"

namespace toycc::ir {
    // ------------ ScopeFrame
    ScopeFrame::ScopeFrame(ScopeStack& scope_stack, std::shared_ptr<Scope> scope) : scope_stack(scope_stack) {
        scope_stack.push_back(scope);
    }

    ScopeFrame::~ScopeFrame() {
        scope_stack.pop_back();
    }
}
