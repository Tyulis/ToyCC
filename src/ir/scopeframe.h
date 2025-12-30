#pragma once

#include <deque>
#include <memory>

#include "ir/scope.h"

namespace toycc::ir {
    using ScopeStack = std::deque<std::shared_ptr<Scope>>;

    // -------- RAII class for pushing and popping scopes off the scope stack -> ir/generator/scopeframe.cpp
    class ScopeFrame {
        public:
            ScopeFrame(ScopeStack& scope_stack, std::shared_ptr<Scope> scope);
            ~ScopeFrame();

        private:
            ScopeStack& scope_stack;
    };
}
