#pragma once

#include <string>
#include <sstream>

namespace toycc {
    // x86_64 assembly code formatting helper
    class CodeOutput {
        public:
            void label(std::string name, std::optional<std::string> comment = {});
            void statement(std::string code, std::optional<std::string> comment = {});
            void labeled_statement(std::string label, std::string code, std::optional<std::string> comment = {});
            void directive(std::string code, std::optional<std::string> comment = {});
            void comment(std::string content);
            void debug(std::string content, std::optional<std::string> comment = {});

            std::string str() const;

            CodeOutput& operator<< (const CodeOutput& code);
            CodeOutput& operator<< (const std::string& code);

        private:
            std::stringstream output;
    };

    std::ostream& operator<< (std::ostream& output, const CodeOutput& code);
}
