#pragma once

#include <string>
#include <unordered_map>

#include "output.h"

namespace toycc::debug {
    class CompilationUnit {
        public:
            size_t fileno(std::string filename);  // Get the fileno for the given filename, add it if it's not known yet
            void emit_filenos(CodeOutput& assembly);

        private:
            std::unordered_map<std::string, size_t> filenos;  // File numbers for the `.loc` directives
            size_t current_fileno = 0;
    };
}
