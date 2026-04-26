#include "config.h"
#include "output.h"
#include "util/strings.h"

namespace toycc {
    // -------- CodeOutput
    void CodeOutput::label(std::string name, std::optional<std::string> comment) {
        output << name << ":";
        if (comment.has_value())
            output << "  # " << comment.value();
        output << "\n";
    }

    void CodeOutput::statement(std::string code, std::optional<std::string> comment) {
        output << "\t" << code;
        if (comment.has_value())
            output << "  # " << comment.value();
        output << "\n";
    }

    void CodeOutput::labeled_statement(std::string label, std::string code, std::optional<std::string> comment) {
        output << label << ":\t" << code;
        if (comment.has_value())
            output << "  # " << comment.value();
        output << "\n";
    }

    void CodeOutput::directive(std::string code, std::optional<std::string> comment) {
        output << "\t" << code;
        if (comment.has_value())
            output << "  # " << comment.value();
        output << "\n";
    }

    void CodeOutput::comment(std::string content) {
        std::vector<std::string> lines = split(content, "\n");
        for (const std::string& line : lines)
            output << "\t" << "# " << line << "\n";
    }

    void CodeOutput::debug(std::string content, std::optional<std::string> comment) {
        if (!config::debug::enable)
            return;

        output << "\t" << content;
        if (comment.has_value())
            output << "  # " << comment.value();
        output << "\n";
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
