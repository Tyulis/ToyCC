#pragma once

#include "arch/codegen.h"
#include "ir/scope.h"

namespace toycc::arch::x86_64 {
    using namespace toycc::ir;

    class CodeGenerator : public toycc::arch::CodeGenerator {
        public:
            // -------- Exported methods -> arch/x86_64/exports.cpp
            CodeGenerator(std::shared_ptr<Scope> scope);
            virtual void operator() (std::ostream& output) override;
    };
}
