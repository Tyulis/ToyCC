#include "arch/x86_64/output.h"
#include "util/strings.h"

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

    void CodeOutput::comment(std::string content) {
        std::vector<std::string> lines = split(content, "\n");
        for (const std::string& line : lines)
            output << "\t" << "# " << line << "\n";
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

    std::ostream& operator<< (std::ostream& output, const CodeOutput& code) {
        output << code.str();
        return output;
    }
}
