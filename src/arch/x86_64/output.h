#pragma once

#include <string>
#include <sstream>

#include "ir/allocation.h"

namespace toycc::arch::x86_64 {
    // x86_64 assembly code formatting helper
    class CodeOutput {
        public:
            void label(std::string name);
            void statement(std::string code);
            void directive(std::string code);

            std::string str() const;

            CodeOutput& operator<< (const CodeOutput& code);
            CodeOutput& operator<< (const std::string& code);

        private:
            std::stringstream output;
    };

    // Stack frame object that automatically generates its frame push and pop code
    struct StackFrame : ir::StackFrame {
        CodeOutput output;

        std::string str() const;
    };

    std::ostream& operator<< (std::ostream& output, const CodeOutput& code);
    std::ostream& operator<< (std::ostream& output, const StackFrame& code);
    CodeOutput& operator<< (CodeOutput& output, const StackFrame& code);
}
