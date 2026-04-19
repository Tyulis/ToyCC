#include "config.h"
#include "output.h"
#include "util/strings.h"

namespace toycc {
    // -------- CodeOutput
    void CodeOutput::label(std::string name) {
        output << name << ":\n";
    }

    void CodeOutput::statement(std::string code) {
        output << "\t" << code << "\n";
    }

    void CodeOutput::labeled_statement(std::string label, std::string code) {
        output << label << ":\t" << code << "\n";
    }

    void CodeOutput::directive(std::string code) {
        output << "\t" << code << "\n";
    }

    void CodeOutput::comment(std::string content) {
        std::vector<std::string> lines = split(content, "\n");
        for (const std::string& line : lines)
            output << "\t" << "# " << line << "\n";
    }

    void CodeOutput::debug(std::string content) {
        if (config::debug::enable)
            output << "\t" << content << "\n";
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
