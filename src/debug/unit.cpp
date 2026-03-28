#include "debug/unit.h"

namespace toycc::debug {
    size_t CompilationUnit::fileno(std::string filename) {
        auto it = filenos.find(filename);
        if (it == filenos.end()) {
            size_t new_fileno = current_fileno++;
            filenos[filename] = new_fileno;
            return new_fileno;
        } else {
            return it->second;
        }
    }

    void CompilationUnit::emit_filenos(CodeOutput& assembly) {
        for (const auto& [filename, fileno] : filenos)
            assembly.debug(std::format(".file {} \"{}\"", fileno, filename));
    }
}
