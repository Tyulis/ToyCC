#include "ir/generator.h"

namespace toycc::ir {
    // ------------ ScopeFrame
    Generator::ScopeFrame::ScopeFrame(std::deque<std::shared_ptr<Scope>>& scope_stack, std::shared_ptr<Scope> scope) : scope_stack(scope_stack) {
        scope_stack.push_back(scope);
    }

    Generator::ScopeFrame::~ScopeFrame() {
        scope_stack.pop_back();
    }

    Generator::ScopeFrame Generator::in_scope(std::shared_ptr<Scope> scope) {
        return {scope_stack, scope};
    }
}
