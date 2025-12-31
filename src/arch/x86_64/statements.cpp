#include "arch/x86_64/codegen.h"

namespace toycc::arch::x86_64 {
    void CodeGenerator::generate_marker(std::shared_ptr<Statement> marker) {
        const auto [begin, end] = current_scope()->markers.equal_range(marker);
        for (auto label_it = begin; label_it != end; label_it++)
            write_label(label_it->second->name);
    }
}
