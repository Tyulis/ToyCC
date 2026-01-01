#include "arch/x86_64/codegen.h"
#include "diagnostic.h"

namespace toycc::arch::x86_64 {
    void CodeGenerator::generate_marker(std::shared_ptr<Statement> marker) {
        throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Marker generation is not implemented");
        /*const auto [begin, end] = current_scope()->markers.equal_range(marker);
        for (auto label_it = begin; label_it != end; label_it++)
            write_label(label_it->second->name);*/
    }
}
