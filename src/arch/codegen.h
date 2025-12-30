#pragma once

#include <memory>
#include <iostream>

#include "ir/scope.h"

namespace toycc::arch {
    class CodeGenerator {
        public:
            CodeGenerator(std::shared_ptr<ir::Scope> scope);
            virtual void operator() (std::ostream& output) = 0;

        protected:
            std::shared_ptr<ir::Scope> global_scope;
    };
}
