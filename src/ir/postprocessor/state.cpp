#include "ir/postprocessor.h"

namespace toycc::ir {
    std::string PostProcessor::anonymous_identifier() {
        return std::format("@P.I{}", unique_id++);
    }

    std::string PostProcessor::anonymous_label() {
        return std::format("@P.L{}", unique_id++);
    }

    std::string PostProcessor::anonymous_type() {
        return std::format("@P.T{}", unique_id++);
    }

    std::string PostProcessor::make_scope_prefix(std::string name) {
        return std::format("@P.S{}", name);
    }

    std::string PostProcessor::make_scope_prefix() {
        return std::format("@P.S{}", unique_id++);
    }
}
