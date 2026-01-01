#pragma once

#include <iostream>

#include "ir/flow.h"

namespace toycc::arch {
    class CodeGenerator {
        public:
            CodeGenerator(const ir::TranslationUnit& unit);
            virtual void operator() (std::ostream& output) = 0;

        protected:
            ir::TranslationUnit unit;
    };
}
