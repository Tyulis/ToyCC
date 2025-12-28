#include "diagnostic.h"
#include "ir/generator.h"

namespace toycc::ir {
    // ------------ Exported and public functions
    std::shared_ptr<Scope> generate_ir(const SourceMap& source_map, CParser::CompilationUnitContext* context) {
        Generator generator(source_map, context);
        return generator.get();
    }

    Generator::Generator(const SourceMap& source_map, CParser::CompilationUnitContext* context) : source_map(source_map) {
        init_global_scope();
        decode_compilation_unit(context);
    }

    std::shared_ptr<Scope> Generator::get() {
        if (scope_stack.size() != 1)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Found scopes other than the global scope in the scope stack after decoding ended");
        return scope_stack[0];
    }
}
