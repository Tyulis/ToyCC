#pragma once

#include <string>
#include <sstream>

namespace toycc {
    // x86_64 assembly code formatting helper
    class CodeOutput {
        public:
            void label(std::string name);
            void statement(std::string code);
            void labeled_statement(std::string label, std::string code);
            void directive(std::string code);
            void comment(std::string content);
            void debug(std::string content);

            std::string str() const;

            CodeOutput& operator<< (const CodeOutput& code);
            CodeOutput& operator<< (const std::string& code);

        private:
            std::stringstream output;
    };

    std::ostream& operator<< (std::ostream& output, const CodeOutput& code);
}
