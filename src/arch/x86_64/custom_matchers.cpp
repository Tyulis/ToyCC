#include "arch/x86_64/custom_matchers.h"

namespace toycc::arch::x86_64 {
    std::optional<TranslationMatch> match_call(const StackFrame&, const ir::DependencyGraph&, const GroupMatch&) {
        return {};
    }
}
