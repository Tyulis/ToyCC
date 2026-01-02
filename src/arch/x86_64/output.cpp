#include "arch/x86_64/output.h"

namespace toycc::arch::x86_64 {
    // -------- CodeOutput
    void CodeOutput::label(std::string name) {
        output << name << ":\n";
    }

    void CodeOutput::statement(std::string code) {
        output << "\t" << code << "\n";
    }

    void CodeOutput::directive(std::string code) {
        output << "\t" << code << "\n";
    }

    std::string CodeOutput::str() const {
        return output.str();
    }

    CodeOutput& CodeOutput::operator<< (const CodeOutput& code) {
        output << code.str();
        return *this;
    }

    CodeOutput& CodeOutput::operator<< (const std::string& code) {
        output << code;
        return *this;
    }



    // -------- StackFrame
    std::string StackFrame::str() const {
        CodeOutput code;

        code.statement("pushq %rbp");
        code.statement("movq %rsp, %rbp");
        if (current_position > 0)
            code.statement(std::format("subq ${}, %rsp", current_position));

        code << output;

        code.statement("popq %rbp");
        code.statement("ret");

        return code.str();
    }

    // -------- Stream operators
    std::ostream& operator<< (std::ostream& output, const CodeOutput& code) {
        output << code.str();
        return output;
    }

    std::ostream& operator<< (std::ostream& output, const StackFrame& code) {
        output << code.str();
        return output;
    }

    CodeOutput& operator<< (CodeOutput& output, const StackFrame& code) {
        output << code.str();
        return output;
    }

}
