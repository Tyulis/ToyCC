#pragma once

#include <iostream>

#include "flow/unit.h"

namespace toycc::arch {
    class CodeGenerator {
        public:
            CodeGenerator(const flow::TranslationUnit& unit);
            virtual void operator() (std::ostream& output) = 0;

        protected:
            flow::TranslationUnit unit;
    };
}
